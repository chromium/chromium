// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/sad_plugin.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebPluginContainer;
using blink::WebPoint;
using blink::WebRect;
using blink::WebURL;
using blink::WebVector;

namespace {

using PluginContainerMap =
    std::map<blink::WebPluginContainer*, content::BrowserPlugin*>;
static base::LazyInstance<PluginContainerMap>::DestructorAtExit
    g_plugin_container_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content {

// static
BrowserPlugin* BrowserPlugin::GetFromNode(const blink::WebNode& node) {
  blink::WebPluginContainer* container = node.PluginContainer();
  if (!container)
    return nullptr;

  PluginContainerMap* browser_plugins = g_plugin_container_map.Pointer();
  auto it = browser_plugins->find(container);
  return it == browser_plugins->end() ? nullptr : it->second;
}

BrowserPlugin::BrowserPlugin(
    RenderFrame* render_frame,
    const base::WeakPtr<BrowserPluginDelegate>& delegate)
    : attached_(false),
      render_frame_routing_id_(render_frame->GetRoutingID()),
      container_(nullptr),
      guest_crashed_(false),
      plugin_focused_(false),
      visible_(true),
      mouse_locked_(false),
      ready_(false),
      browser_plugin_instance_id_(browser_plugin::kInstanceIDNone),
      delegate_(delegate),
      task_runner_(
          // Make sure BrowserPlugin::UpdateInternalInstanceId runs in FIFO
          // order with respect to other loading tasks.
          render_frame->GetTaskRunner(blink::TaskType::kInternalLoading)) {
  browser_plugin_instance_id_ =
      BrowserPluginManager::Get()->GetNextInstanceID();

  // TODO(jonross): Address the Surface Invariants Violations that led to the
  // addition of ParentLocalSurfaceIdAllocator::Reset used within
  // BrowserPlugin::OnAttachACK. Then have BrowserPlugin only generate new ids
  // as needed.
  parent_local_surface_id_allocator_.GenerateId();

  if (delegate_)
    delegate_->SetElementInstanceID(browser_plugin_instance_id_);
}

BrowserPlugin::~BrowserPlugin() {
  Detach();

  if (delegate_) {
    delegate_->DidDestroyElement();
    delegate_.reset();
  }

  BrowserPluginManager::Get()->RemoveBrowserPlugin(browser_plugin_instance_id_);
}

bool BrowserPlugin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPlugin, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_Attach_ACK, OnAttachACK)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestGone, OnGuestGone)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestReady, OnGuestReady)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_EnableAutoResize, OnEnableAutoResize)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_DisableAutoResize, OnDisableAutoResize)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_DidUpdateVisualProperties,
                        OnDidUpdateVisualProperties)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetMouseLock, OnSetMouseLock)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_ShouldAcceptTouchEvents,
                        OnShouldAcceptTouchEvents)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPlugin::UpdateDOMAttribute(const std::string& attribute_name,
                                       const base::string16& attribute_value) {
  if (!Container())
    return;

  blink::WebElement element = Container()->GetElement();
  element.SetAttribute(blink::WebString::FromUTF8(attribute_name),
                       blink::WebString::FromUTF16(attribute_value));
}

void BrowserPlugin::Attach() {
  Detach();

  BrowserPluginHostMsg_Attach_Params attach_params;
  attach_params.focused = ShouldGuestBeFocused();
  attach_params.visible = visible_;
  attach_params.frame_rect = screen_space_rect();
  attach_params.is_full_page_plugin = false;
  if (Container()) {
    blink::WebLocalFrame* frame = Container()->GetDocument().GetFrame();
    attach_params.is_full_page_plugin =
        frame->View()->MainFrame()->IsWebLocalFrame() &&
        frame->View()
            ->MainFrame()
            ->ToWebLocalFrame()
            ->GetDocument()
            .IsPluginDocument();
  }
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_Attach(
      render_frame_routing_id_,
      browser_plugin_instance_id_,
      attach_params));

  // Post an update event to the associated accessibility object.
  auto* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  if (render_frame && render_frame->render_accessibility() && Container()) {
    blink::WebElement element = Container()->GetElement();
    blink::WebAXObject ax_element = blink::WebAXObject::FromWebNode(element);
    if (!ax_element.IsDetached()) {
      render_frame->render_accessibility()->HandleAXEvent(
          ax_element, ax::mojom::Event::kChildrenChanged);
    }
  }

  sent_visual_properties_ = base::nullopt;
}

void BrowserPlugin::Detach() {
  if (!attached())
    return;

  attached_ = false;
  guest_crashed_ = false;
  embedded_layer_ = nullptr;

  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_Detach(browser_plugin_instance_id_));
}

const viz::LocalSurfaceIdAllocation&
BrowserPlugin::GetLocalSurfaceIdAllocation() const {
  return parent_local_surface_id_allocator_
      .GetCurrentLocalSurfaceIdAllocation();
}

void BrowserPlugin::SynchronizeVisualProperties() {
  bool size_changed = !sent_visual_properties_ ||
                      sent_visual_properties_->auto_resize_enabled !=
                          pending_visual_properties_.auto_resize_enabled ||
                      sent_visual_properties_->min_size_for_auto_resize !=
                          pending_visual_properties_.min_size_for_auto_resize ||
                      sent_visual_properties_->max_size_for_auto_resize !=
                          pending_visual_properties_.max_size_for_auto_resize ||
                      sent_visual_properties_->local_frame_size !=
                          pending_visual_properties_.local_frame_size ||
                      sent_visual_properties_->screen_space_rect.size() !=
                          pending_visual_properties_.screen_space_rect.size();

  bool zoom_changed =
      !sent_visual_properties_ || sent_visual_properties_->zoom_level !=
                                      pending_visual_properties_.zoom_level;

  // Note that the following flag is true if the capture sequence number
  // actually changed. That is, it is false if we did not have
  // |sent_visual_properties_|, which is different from the other local flags
  // here.
  bool capture_sequence_number_changed =
      sent_visual_properties_ &&
      sent_visual_properties_->capture_sequence_number !=
          pending_visual_properties_.capture_sequence_number;

  bool synchronized_props_changed =
      !sent_visual_properties_ || size_changed || zoom_changed ||
      sent_visual_properties_->screen_info !=
          pending_visual_properties_.screen_info ||
      capture_sequence_number_changed;

  if (synchronized_props_changed) {
    parent_local_surface_id_allocator_.GenerateId();
    pending_visual_properties_.local_surface_id_allocation =
        parent_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
  }

  if (frame_sink_id_.is_valid()) {
    // If we're synchronizing surfaces, then use an infinite deadline to ensure
    // everything is synchronized.
    cc::DeadlinePolicy deadline =
        capture_sequence_number_changed
            ? cc::DeadlinePolicy::UseInfiniteDeadline()
            : cc::DeadlinePolicy::UseDefaultDeadline();
    compositing_helper_->SetSurfaceId(
        viz::SurfaceId(frame_sink_id_,
                       GetLocalSurfaceIdAllocation().local_surface_id()),
        screen_space_rect().size(), deadline);
  }

  bool position_changed =
      !sent_visual_properties_ ||
      sent_visual_properties_->screen_space_rect.origin() !=
          pending_visual_properties_.screen_space_rect.origin();
  bool visual_properties_changed =
      synchronized_props_changed || position_changed;

  if (visual_properties_changed && attached()) {
    // Let the browser know about the updated view rect.
    BrowserPluginManager::Get()->Send(
        new BrowserPluginHostMsg_SynchronizeVisualProperties(
            browser_plugin_instance_id_, pending_visual_properties_));
  }

  if (delegate_ && size_changed)
    delegate_->DidResizeElement(screen_space_rect().size());

  if (visual_properties_changed && attached())
    sent_visual_properties_ = pending_visual_properties_;
}

void BrowserPlugin::OnAdvanceFocus(int browser_plugin_instance_id,
                                   bool reverse) {
  auto* render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto* render_view = render_frame ? render_frame->GetRenderView() : nullptr;
  if (!render_view)
    return;
  render_view->GetWebView()->AdvanceFocus(reverse);
}

void BrowserPlugin::OnAttachACK(int browser_plugin_instance_id) {
  attached_ = true;
  SynchronizeVisualProperties();
}

void BrowserPlugin::OnGuestGone(int browser_plugin_instance_id) {
  guest_crashed_ = true;
  compositing_helper_->ChildFrameGone(screen_space_rect().size(),
                                      screen_info().device_scale_factor);
}

void BrowserPlugin::OnGuestReady(int browser_plugin_instance_id,
                                 const viz::FrameSinkId& frame_sink_id) {
  guest_crashed_ = false;
  frame_sink_id_ = frame_sink_id;
  sent_visual_properties_ = base::nullopt;
  SynchronizeVisualProperties();
}

void BrowserPlugin::OnDidUpdateVisualProperties(
    int browser_plugin_instance_id,
    const cc::RenderFrameMetadata& metadata) {
  if (!parent_local_surface_id_allocator_.UpdateFromChild(
          metadata.local_surface_id_allocation.value_or(
              viz::LocalSurfaceIdAllocation()))) {
    return;
  }

  // The viz::LocalSurfaceId has changed so we call SynchronizeVisualProperties
  // here to embed it.
  SynchronizeVisualProperties();
}

void BrowserPlugin::OnEnableAutoResize(int browser_plugin_instance_id,
                                       const gfx::Size& min_size,
                                       const gfx::Size& max_size) {
  pending_visual_properties_.auto_resize_enabled = true;
  pending_visual_properties_.min_size_for_auto_resize = min_size;
  pending_visual_properties_.max_size_for_auto_resize = max_size;
  SynchronizeVisualProperties();
}

void BrowserPlugin::OnDisableAutoResize(int browser_plugin_instance_id) {
  pending_visual_properties_.auto_resize_enabled = false;
  SynchronizeVisualProperties();
}

void BrowserPlugin::OnSetCursor(int browser_plugin_instance_id,
                                const WebCursor& cursor) {
  cursor_ = cursor;
}

void BrowserPlugin::OnSetMouseLock(int browser_plugin_instance_id,
                                   bool enable) {
  RenderWidget* render_widget = embedding_render_widget_.get();
  if (enable) {
    if (mouse_locked_ || !render_widget)
      return;
    blink::WebLocalFrame* requester_frame =
        Container()->GetDocument().GetFrame();
    render_widget->mouse_lock_dispatcher()->LockMouse(
        this, requester_frame, false /*request_unadjusted_movement*/);
  } else {
    if (!mouse_locked_) {
      OnLockMouseACK(false);
      return;
    }
    if (!render_widget)
      return;
    render_widget->mouse_lock_dispatcher()->UnlockMouse(this);
  }
}

void BrowserPlugin::OnShouldAcceptTouchEvents(int browser_plugin_instance_id,
                                              bool accept) {
  if (Container()) {
    Container()->RequestTouchEventType(
        accept ? WebPluginContainer::kTouchEventRequestTypeRaw
               : WebPluginContainer::kTouchEventRequestTypeNone);
  }
}

void BrowserPlugin::UpdateInternalInstanceId() {
  // This is a way to notify observers of our attributes that this plugin is
  // available in render tree.
  // TODO(lazyboy): This should be done through the delegate instead. Perhaps
  // by firing an event from there.
  UpdateDOMAttribute(
      "internalinstanceid",
      base::UTF8ToUTF16(base::NumberToString(browser_plugin_instance_id_)));
}

void BrowserPlugin::UpdateGuestFocusState(blink::WebFocusType focus_type) {
  if (!attached())
    return;
  bool should_be_focused = ShouldGuestBeFocused();
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetFocus(
      browser_plugin_instance_id_,
      should_be_focused,
      focus_type));
}

void BrowserPlugin::ScreenInfoChanged(const ScreenInfo& screen_info) {
  pending_visual_properties_.screen_info = screen_info;
  if (guest_crashed_) {
    // Update the sad page to match the current ScreenInfo.
    compositing_helper_->ChildFrameGone(screen_space_rect().size(),
                                        screen_info.device_scale_factor);
    return;
  }
  SynchronizeVisualProperties();
}

void BrowserPlugin::OnZoomLevelChanged(double zoom_level) {
  pending_visual_properties_.zoom_level = zoom_level;
  SynchronizeVisualProperties();
}

void BrowserPlugin::UpdateCaptureSequenceNumber(
    uint32_t capture_sequence_number) {
  pending_visual_properties_.capture_sequence_number = capture_sequence_number;
  SynchronizeVisualProperties();
}

bool BrowserPlugin::ShouldGuestBeFocused() const {
  bool embedder_focused = false;
  if (embedding_render_widget_)
    embedder_focused = embedding_render_widget_->has_focus();
  return plugin_focused_ && embedder_focused;
}

WebPluginContainer* BrowserPlugin::Container() const {
  return container_;
}

bool BrowserPlugin::Initialize(WebPluginContainer* container) {
  DCHECK(container);
  DCHECK_EQ(this, container->Plugin());

  container_ = container;
  container_->SetWantsWheelEvents(true);

  g_plugin_container_map.Get().insert(std::make_pair(container_, this));

  BrowserPluginManager::Get()->AddBrowserPlugin(
      browser_plugin_instance_id_, this);

  // Defer attach call so that if there's any pending browser plugin
  // destruction, then it can progress first.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BrowserPlugin::UpdateInternalInstanceId,
                                weak_ptr_factory_.GetWeakPtr()));

  compositing_helper_ = std::make_unique<ChildFrameCompositingHelper>(this);

  DCHECK_EQ(RenderFrameImpl::FromWebFrame(container_->GetDocument().GetFrame()),
            RenderFrameImpl::FromRoutingID(render_frame_routing_id()));
  embedding_render_widget_ =
      RenderFrameImpl::FromWebFrame(container_->GetDocument().GetFrame())
          ->GetLocalRootRenderWidget()
          ->AsWeakPtr();
  embedding_render_widget_->RegisterBrowserPlugin(this);

  return true;
}

void BrowserPlugin::Destroy() {
  if (embedding_render_widget_) {
    embedding_render_widget_->UnregisterBrowserPlugin(this);
    embedding_render_widget_ = nullptr;
  }

  if (container_) {
    // The BrowserPlugin's WebPluginContainer is deleted immediately after this
    // call returns, so let's not keep a reference to it around.
    g_plugin_container_map.Get().erase(container_);
  }

  container_ = nullptr;
  // Will be a no-op if the mouse is not currently locked.
  if (embedding_render_widget_) {
    embedding_render_widget_->mouse_lock_dispatcher()->OnLockTargetDestroyed(
        this);
  }

  task_runner_->DeleteSoon(FROM_HERE, this);
}

v8::Local<v8::Object> BrowserPlugin::V8ScriptableObject(v8::Isolate* isolate) {
  if (!delegate_)
    return v8::Local<v8::Object>();

  return delegate_->V8ScriptableObject(isolate);
}

bool BrowserPlugin::SupportsKeyboardFocus() const {
  return visible_;
}

bool BrowserPlugin::SupportsEditCommands() const {
  return true;
}

bool BrowserPlugin::SupportsInputMethod() const {
  return true;
}

bool BrowserPlugin::CanProcessDrag() const {
  return true;
}

// static
bool BrowserPlugin::ShouldForwardToBrowserPlugin(
    const IPC::Message& message) {
  return IPC_MESSAGE_CLASS(message) == BrowserPluginMsgStart;
}

void BrowserPlugin::UpdateGeometry(const WebRect& plugin_rect_in_viewport,
                                   const WebRect& clip_rect,
                                   const WebRect& unobscured_rect,
                                   bool is_visible) {
  // Ignore this call during teardown. If the embedding RenderWidget is gone,
  // don't bother sending new geometry to the child because it's not being shown
  // anymore.
  if (!embedding_render_widget_)
    return;

  // Convert the plugin_rect_in_viewport to window coordinates, which is css.
  WebRect rect_in_css(plugin_rect_in_viewport);

  // We will use the local root's RenderWidget to convert coordinates to Window.
  // If this local root belongs to an OOPIF, on the browser side we will have to
  // consider the displacement of the child frame in root window.
  embedding_render_widget_->ConvertViewportToWindow(&rect_in_css);
  gfx::Rect screen_space_rect = rect_in_css;

  if (!ready_) {
    if (delegate_)
      delegate_->Ready();
    ready_ = true;
  }

  pending_visual_properties_.screen_space_rect = screen_space_rect;
  if (guest_crashed_) {
    // Update the sad page to match the current ScreenInfo.
    compositing_helper_->ChildFrameGone(screen_space_rect.size(),
                                        screen_info().device_scale_factor);
    return;
  }
  SynchronizeVisualProperties();
}

void BrowserPlugin::UpdateFocus(bool focused, blink::WebFocusType focus_type) {
  plugin_focused_ = focused;
  UpdateGuestFocusState(focus_type);
}

void BrowserPlugin::UpdateVisibility(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (!attached())
    return;

  compositing_helper_->UpdateVisibility(visible);

  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetVisibility(
      browser_plugin_instance_id_,
      visible));
}

blink::WebInputEventResult BrowserPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
    blink::WebCursorInfo& cursor_info) {
  const blink::WebInputEvent& event = coalesced_event.Event();
  if (guest_crashed_ || !attached())
    return blink::WebInputEventResult::kNotHandled;

  DCHECK(!blink::WebInputEvent::IsTouchEventType(event.GetType()));

  // With direct event routing turned on, BrowserPlugin should almost never
  // see wheel events any more. The two exceptions are (1) scroll bubbling, and
  // (2) synthetic mouse wheels generated by touchpad GesturePinch events on
  // Mac, which always go to the mainframe and thus may hit BrowserPlugin if
  // it's in a top-level embedder. In both cases we should indicate the event
  // as not handled (for GesturePinch on Mac, indicating the event has been
  // handled leads to touchpad pinch not working).
  if (event.GetType() == blink::WebInputEvent::kMouseWheel)
    return blink::WebInputEventResult::kNotHandled;

  if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    auto gesture_event = static_cast<const blink::WebGestureEvent&>(event);

    // We shouldn't be forwarding GestureEvents to the Guest anymore. Indicate
    // we handled this only if it's a non-resent event.
    return gesture_event.resending_plugin_id == browser_plugin_instance_id_
               ? blink::WebInputEventResult::kNotHandled
               : blink::WebInputEventResult::kHandledApplication;
  }

  if (event.GetType() == blink::WebInputEvent::kContextMenu)
    return blink::WebInputEventResult::kHandledSuppressed;

  if (blink::WebInputEvent::IsKeyboardEventType(event.GetType()) &&
      !edit_commands_.empty()) {
    BrowserPluginManager::Get()->Send(
        new BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent(
            browser_plugin_instance_id_,
            edit_commands_));
    edit_commands_.clear();
  }

  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(browser_plugin_instance_id_,
                                                &event));
  cursor_info = cursor_.info().GetWebCursorInfo();

  // Although we forward this event to the guest, we don't report it as consumed
  // since other targets of this event in Blink never get that chance either.
  if (event.GetType() == blink::WebInputEvent::kGestureFlingStart)
    return blink::WebInputEventResult::kNotHandled;

  return blink::WebInputEventResult::kHandledApplication;
}

bool BrowserPlugin::HandleDragStatusUpdate(blink::WebDragStatus drag_status,
                                           const blink::WebDragData& drag_data,
                                           blink::WebDragOperationsMask mask,
                                           const blink::WebFloatPoint& position,
                                           const blink::WebFloatPoint& screen) {
  if (guest_crashed_ || !attached())
    return false;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_DragStatusUpdate(
        browser_plugin_instance_id_,
        drag_status,
        DropDataBuilder::Build(drag_data),
        mask,
        position));
  return true;
}

void BrowserPlugin::DidReceiveResponse(const blink::WebURLResponse& response) {}

void BrowserPlugin::DidReceiveData(const char* data, size_t data_length) {
  if (delegate_)
    delegate_->PluginDidReceiveData(data, data_length);
}

void BrowserPlugin::DidFinishLoading() {
  if (delegate_)
    delegate_->PluginDidFinishLoading();
}

void BrowserPlugin::DidFailLoading(const blink::WebURLError& error) {}

bool BrowserPlugin::ExecuteEditCommand(const blink::WebString& name) {
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ExecuteEditCommand(
      browser_plugin_instance_id_, name.Utf8()));

  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::ExecuteEditCommand(const blink::WebString& name,
                                       const blink::WebString& value) {
  edit_commands_.push_back(EditCommand(name.Utf8(), value.Utf8()));
  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::SetComposition(
    const blink::WebString& text,
    const blink::WebVector<blink::WebImeTextSpan>& ime_text_spans,
    const blink::WebRange& replacementRange,
    int selectionStart,
    int selectionEnd) {
  if (!attached())
    return false;

  BrowserPluginHostMsg_SetComposition_Params params;
  params.text = text.Utf16();
  for (size_t i = 0; i < ime_text_spans.size(); ++i) {
    params.ime_text_spans.push_back(ime_text_spans[i]);
  }

  params.replacement_range =
      replacementRange.IsNull()
          ? gfx::Range::InvalidRange()
          : gfx::Range(static_cast<uint32_t>(replacementRange.StartOffset()),
                       static_cast<uint32_t>(replacementRange.EndOffset()));
  params.selection_start = selectionStart;
  params.selection_end = selectionEnd;

  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ImeSetComposition(
      browser_plugin_instance_id_, params));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

bool BrowserPlugin::CommitText(
    const blink::WebString& text,
    const blink::WebVector<blink::WebImeTextSpan>& ime_text_spans,
    const blink::WebRange& replacementRange,
    int relative_cursor_pos) {
  if (!attached())
    return false;

  std::vector<blink::WebImeTextSpan> std_ime_text_spans;
  for (size_t i = 0; i < ime_text_spans.size(); ++i) {
    std_ime_text_spans.push_back(ime_text_spans[i]);
  }
  gfx::Range replacement_range =
      replacementRange.IsNull()
          ? gfx::Range::InvalidRange()
          : gfx::Range(static_cast<uint32_t>(replacementRange.StartOffset()),
                       static_cast<uint32_t>(replacementRange.EndOffset()));

  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ImeCommitText(
      browser_plugin_instance_id_, text.Utf16(), std_ime_text_spans,
      replacement_range, relative_cursor_pos));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

bool BrowserPlugin::FinishComposingText(
    blink::WebInputMethodController::ConfirmCompositionBehavior
        selection_behavior) {
  if (!attached())
    return false;
  bool keep_selection =
      (selection_behavior == blink::WebInputMethodController::kKeepSelection);
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_ImeFinishComposingText(
          browser_plugin_instance_id_, keep_selection));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

void BrowserPlugin::ExtendSelectionAndDelete(int before, int after) {
  if (!attached())
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_ExtendSelectionAndDelete(
          browser_plugin_instance_id_,
          before,
          after));
}

void BrowserPlugin::OnLockMouseACK(bool succeeded) {
  mouse_locked_ = succeeded;
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_LockMouse_ACK(
      browser_plugin_instance_id_,
      succeeded));
}

void BrowserPlugin::OnMouseLockLost() {
  mouse_locked_ = false;
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_UnlockMouse_ACK(
      browser_plugin_instance_id_));
}

bool BrowserPlugin::HandleMouseLockedInputEvent(
    const blink::WebMouseEvent& event) {
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(browser_plugin_instance_id_,
                                                &event));
  return true;
}

cc::Layer* BrowserPlugin::GetLayer() {
  return embedded_layer_.get();
}

void BrowserPlugin::SetLayer(scoped_refptr<cc::Layer> layer,
                             bool prevent_contents_opaque_changes,
                             bool is_surface_layer) {
  if (container_)
    container_->SetCcLayer(layer.get(), prevent_contents_opaque_changes);
  embedded_layer_ = std::move(layer);
}

SkBitmap* BrowserPlugin::GetSadPageBitmap() {
  return GetContentClient()->renderer()->GetSadWebViewBitmap();
}

}  // namespace content
