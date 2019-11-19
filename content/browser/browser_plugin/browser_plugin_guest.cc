// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/surfaces/surface.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_guest.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_visual_properties.h"
#include "content/common/input/ime_text_span_conversions.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/guest_host.h"
#include "content/public/browser/guest_mode.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/drop_data.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/size_conversions.h"

#if defined(OS_MACOSX)
#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"
#include "content/common/frame_messages.h"
#endif

namespace content {

BrowserPluginGuest::InputEventShimImpl::InputEventShimImpl(
    BrowserPluginGuest* browser_plugin_guest)
    : browser_plugin_guest_(browser_plugin_guest) {}

BrowserPluginGuest::InputEventShimImpl::~InputEventShimImpl() = default;

void BrowserPluginGuest::InputEventShimImpl::DidSetHasTouchEventHandlers(
    bool accept) {
  browser_plugin_guest_->DidSetHasTouchEventHandlers(accept);
}

void BrowserPluginGuest::InputEventShimImpl::DidTextInputStateChange(
    const TextInputState& params) {
  browser_plugin_guest_->DidTextInputStateChange(params);
}

void BrowserPluginGuest::InputEventShimImpl::DidLockMouse(bool user_gesture,
                                                          bool privileged) {
  browser_plugin_guest_->DidLockMouse(user_gesture, privileged);
}

void BrowserPluginGuest::InputEventShimImpl::DidUnlockMouse() {
  browser_plugin_guest_->DidUnlockMouse();
}

class BrowserPluginGuest::EmbedderVisibilityObserver
    : public WebContentsObserver {
 public:
  explicit EmbedderVisibilityObserver(BrowserPluginGuest* guest)
      : WebContentsObserver(guest->embedder_web_contents()),
        browser_plugin_guest_(guest) {
  }

  ~EmbedderVisibilityObserver() override {}

  // WebContentsObserver implementation.
  void OnVisibilityChanged(content::Visibility visibility) override {
    browser_plugin_guest_->EmbedderVisibilityChanged(visibility);
  }

 private:
  BrowserPluginGuest* browser_plugin_guest_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderVisibilityObserver);
};

BrowserPluginGuest::BrowserPluginGuest(bool has_render_view,
                                       WebContentsImpl* web_contents,
                                       BrowserPluginGuestDelegate* delegate)
    : WebContentsObserver(web_contents),
      input_event_shim_impl_(this),
      owner_web_contents_(nullptr),
      attached_(false),
      browser_plugin_instance_id_(browser_plugin::kInstanceIDNone),
      focused_(false),
      mouse_locked_(false),
      pending_lock_request_(false),
      guest_visible_(false),
      embedder_visibility_(Visibility::VISIBLE),
      is_full_page_plugin_(false),
      has_render_view_(has_render_view),
      is_in_destruction_(false),
      initialized_(false),
      guest_render_view_routing_id_(MSG_ROUTING_NONE),
      last_drag_status_(blink::kWebDragStatusUnknown),
      seen_embedder_system_drag_ended_(false),
      seen_embedder_drag_source_ended_at_(false),
      ignore_dragged_url_(true),
      delegate_(delegate),
      can_use_cross_process_frames_(delegate->CanUseCrossProcessFrames()) {
  DCHECK(web_contents);
  DCHECK(delegate);
  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Create"));
}

int BrowserPluginGuest::LoadURLWithParams(
    const NavigationController::LoadURLParams& load_params) {
  GetWebContents()->GetController().LoadURLWithParams(load_params);
  return GetGuestRenderViewRoutingID();
}

void BrowserPluginGuest::EnableAutoResize(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  SendMessageToEmbedder(std::make_unique<BrowserPluginMsg_EnableAutoResize>(
      browser_plugin_instance_id_, min_size, max_size));
}

void BrowserPluginGuest::DisableAutoResize() {
  SendMessageToEmbedder(std::make_unique<BrowserPluginMsg_DisableAutoResize>(
      browser_plugin_instance_id_));
}

void BrowserPluginGuest::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  SendMessageToEmbedder(
      std::make_unique<BrowserPluginMsg_DidUpdateVisualProperties>(
          browser_plugin_instance_id_, metadata));
}

void BrowserPluginGuest::SizeContents(const gfx::Size& new_size) {
  GetWebContents()->GetView()->SizeContents(new_size);
}

void BrowserPluginGuest::WillDestroy() {
  is_in_destruction_ = true;

  // It is important that the WebContents is notified before detaching.
  GetWebContents()->BrowserPluginGuestWillDetach();

  attached_ = false;
  owner_web_contents_ = nullptr;
}

RenderWidgetHostImpl* BrowserPluginGuest::GetOwnerRenderWidgetHost() const {
  return static_cast<RenderWidgetHostImpl*>(
      delegate_->GetOwnerRenderWidgetHost());
}

RenderFrameHostImpl* BrowserPluginGuest::GetEmbedderFrame() const {
  return static_cast<RenderFrameHostImpl*>(delegate_->GetEmbedderFrame());
}

void BrowserPluginGuest::Init() {
  if (initialized_)
    return;
  initialized_ = true;

  WebContentsImpl* owner_web_contents = static_cast<WebContentsImpl*>(
      delegate_->GetOwnerWebContents());
  owner_web_contents->CreateBrowserPluginEmbedderIfNecessary();
  InitInternal(BrowserPluginHostMsg_Attach_Params(), owner_web_contents);
}

InputEventShim* BrowserPluginGuest::GetInputEventShim() {
  // In --site-per-process mode, the input event mechanics are handled by
  // the RenderWidgetHost so there is no need to shim things.
  if (GuestMode::IsCrossProcessFrameGuest(GetWebContents())) {
    return nullptr;
  }
  return &input_event_shim_impl_;
}

base::WeakPtr<BrowserPluginGuest> BrowserPluginGuest::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BrowserPluginGuest::SetFocus(RenderWidgetHost* rwh,
                                  bool focused,
                                  blink::WebFocusType focus_type) {
  focused_ = focused;
  if (!rwh)
    return;

  if ((focus_type == blink::kWebFocusTypeForward) ||
      (focus_type == blink::kWebFocusTypeBackward)) {
    static_cast<RenderViewHostImpl*>(RenderViewHost::From(rwh))
        ->SetInitialFocus(focus_type == blink::kWebFocusTypeBackward);
  }
  RenderWidgetHostImpl::From(rwh)->GetWidgetInputHandler()->SetFocus(focused);
  if (!focused && mouse_locked_)
    DidUnlockMouse();

  // Restore the last seen state of text input to the view.
  RenderWidgetHostViewBase* rwhv = static_cast<RenderWidgetHostViewBase*>(
      rwh->GetView());
  SendTextInputTypeChangedToView(rwhv);
}

bool BrowserPluginGuest::LockMouse(bool allowed) {
  if (!attached() || (mouse_locked_ == allowed))
    return false;

  return embedder_web_contents()->GotResponseToLockMouseRequest(allowed);
}

WebContentsImpl* BrowserPluginGuest::CreateNewGuestWindow(
    const WebContents::CreateParams& params) {
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(delegate_->CreateNewGuestWindow(params));
  DCHECK(new_contents);
  return new_contents;
}

bool BrowserPluginGuest::OnMessageReceivedFromEmbedder(
    const IPC::Message& message) {
  RenderWidgetHostViewGuest* rwhv = static_cast<RenderWidgetHostViewGuest*>(
      web_contents()->GetRenderWidgetHostView());

  // Until the guest is attached, it should not be handling input events.
  if (attached() && rwhv &&
      rwhv->OnMessageReceivedFromEmbedder(message,
                                          GetOwnerRenderWidgetHost())) {
    return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_DragStatusUpdate,
                        OnDragStatusUpdate)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ExecuteEditCommand,
                        OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ExtendSelectionAndDelete,
                        OnExtendSelectionAndDelete)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ImeCommitText, OnImeCommitText)

    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ImeFinishComposingText,
                        OnImeFinishComposingText)

    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ImeSetComposition,
                        OnImeSetComposition)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_LockMouse_ACK, OnLockMouseAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetVisibility, OnSetVisibility)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UnlockMouse_ACK, OnUnlockMouseAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SynchronizeVisualProperties,
                        OnSynchronizeVisualProperties)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::InitInternal(
    const BrowserPluginHostMsg_Attach_Params& params,
    WebContentsImpl* owner_web_contents) {
  focused_ = params.focused;
  OnSetFocus(browser_plugin::kInstanceIDNone, focused_,
             blink::kWebFocusTypeNone);

  guest_visible_ = params.visible;
  UpdateVisibility();

  is_full_page_plugin_ = params.is_full_page_plugin;
  frame_rect_ = params.frame_rect;

  if (owner_web_contents_ != owner_web_contents) {
    WebContentsViewGuest* new_view = nullptr;
    if (!GuestMode::IsCrossProcessFrameGuest(GetWebContents())) {
      new_view =
          static_cast<WebContentsViewGuest*>(GetWebContents()->GetView());
    }

    if (owner_web_contents_ && new_view)
      new_view->OnGuestDetached(owner_web_contents_->GetView());

    // Once a BrowserPluginGuest has an embedder WebContents, it's considered to
    // be attached.
    owner_web_contents_ = owner_web_contents;
    if (new_view)
      new_view->OnGuestAttached(owner_web_contents_->GetView());
  }

  blink::mojom::RendererPreferences* renderer_prefs =
      GetWebContents()->GetMutableRendererPrefs();
  std::string guest_user_agent_override = renderer_prefs->user_agent_override;
  // Copy renderer preferences (and nothing else) from the embedder's
  // WebContents to the guest.
  //
  // For GTK and Aura this is necessary to get proper renderer configuration
  // values for caret blinking interval, colors related to selection and
  // focus.
  *renderer_prefs = *owner_web_contents_->GetMutableRendererPrefs();
  renderer_prefs->user_agent_override = guest_user_agent_override;

  // Navigation is disabled in Chrome Apps. We want to make sure guest-initiated
  // navigations still continue to function inside the app.
  renderer_prefs->browser_handles_all_top_level_requests = false;
  // Disable "client blocked" error page for browser plugin.
  renderer_prefs->disable_client_blocked_error_page = true;

  embedder_visibility_observer_.reset(new EmbedderVisibilityObserver(this));

  DCHECK(GetWebContents()->GetRenderViewHost());

  // TODO(chrishtr): this code is wrong. The navigate_on_drag_drop field will
  // be reset again the next time preferences are updated.
  WebPreferences prefs =
      GetWebContents()->GetRenderViewHost()->GetWebkitPreferences();
  prefs.navigate_on_drag_drop = false;
  GetWebContents()->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  SendMessageToEmbedder(std::make_unique<BrowserPluginMsg_Attach_ACK>(
      browser_plugin_instance_id()));
}

BrowserPluginGuest::~BrowserPluginGuest() {
}

// static
void BrowserPluginGuest::CreateInWebContents(
    WebContentsImpl* web_contents,
    BrowserPluginGuestDelegate* delegate) {
  auto guest = base::WrapUnique(new BrowserPluginGuest(
      web_contents->HasOpener(), web_contents, delegate));
  delegate->SetGuestHost(guest.get());
  web_contents->SetBrowserPluginGuest(std::move(guest));
}

// static
bool BrowserPluginGuest::IsGuest(WebContentsImpl* web_contents) {
  return web_contents && web_contents->GetBrowserPluginGuest();
}

// static
bool BrowserPluginGuest::IsGuest(RenderViewHostImpl* render_view_host) {
  return render_view_host && IsGuest(
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(
          render_view_host)));
}

RenderWidgetHostView* BrowserPluginGuest::GetOwnerRenderWidgetHostView() {
  if (RenderWidgetHostImpl* owner = GetOwnerRenderWidgetHost())
    return owner->GetView();
  return nullptr;
}

void BrowserPluginGuest::UpdateVisibility() {
  OnSetVisibility(browser_plugin_instance_id(), visible());
}

BrowserPluginGuestManager*
BrowserPluginGuest::GetBrowserPluginGuestManager() const {
  return GetWebContents()->GetBrowserContext()->GetGuestManager();
}

void BrowserPluginGuest::EmbedderVisibilityChanged(Visibility visibility) {
  embedder_visibility_ = visibility;
  UpdateVisibility();
}

void BrowserPluginGuest::PointerLockPermissionResponse(bool allow) {
  SendMessageToEmbedder(std::make_unique<BrowserPluginMsg_SetMouseLock>(
      browser_plugin_instance_id(), allow));
}

void BrowserPluginGuest::ResendEventToEmbedder(
    const blink::WebInputEvent& event) {
  if (!attached() || !owner_web_contents_)
    return;

  DCHECK(browser_plugin_instance_id_);
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(GetOwnerRenderWidgetHostView());

  gfx::Vector2d offset_from_embedder = frame_rect_.OffsetFromOrigin();
  if (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate) {
    blink::WebGestureEvent resent_gesture_event;
    memcpy(&resent_gesture_event, &event, sizeof(blink::WebGestureEvent));
    resent_gesture_event.SetPositionInWidget(
        resent_gesture_event.PositionInWidget() + offset_from_embedder);
    // Mark the resend source with the browser plugin's instance id, so the
    // correct browser_plugin will know to ignore the event.
    resent_gesture_event.resending_plugin_id = browser_plugin_instance_id_;
    ui::LatencyInfo latency_info =
        ui::WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(
            resent_gesture_event);
    // The touch action may not be set for the embedder because the
    // GestureScrollBegin is sent to the guest view. In this case, set the touch
    // action of the embedder to Auto to prevent crash.
    GetOwnerRenderWidgetHost()->input_router()->ForceSetTouchActionAuto();
    view->ProcessGestureEvent(resent_gesture_event, latency_info);
  } else if (event.GetType() == blink::WebInputEvent::kMouseWheel) {
    blink::WebMouseWheelEvent resent_wheel_event;
    memcpy(&resent_wheel_event, &event, sizeof(blink::WebMouseWheelEvent));
    resent_wheel_event.SetPositionInWidget(
        resent_wheel_event.PositionInWidget().x + offset_from_embedder.x(),
        resent_wheel_event.PositionInWidget().y + offset_from_embedder.y());
    resent_wheel_event.resending_plugin_id = browser_plugin_instance_id_;
    // TODO(wjmaclean): Initialize latency info correctly for OOPIFs.
    // https://crbug.com/613628
    ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
    view->ProcessMouseWheelEvent(resent_wheel_event, latency_info);
  } else {
    NOTIMPLEMENTED();
  }
}

gfx::Point BrowserPluginGuest::GetCoordinatesInEmbedderWebContents(
    const gfx::Point& relative_point) {
  RenderWidgetHostView* owner_rwhv = GetOwnerRenderWidgetHostView();
  if (!owner_rwhv)
    return relative_point;

  gfx::Point point(relative_point);

  // Add the offset form the embedder web contents view.
  point += owner_rwhv->TransformPointToRootCoordSpace(frame_rect_.origin())
               .OffsetFromOrigin();
  if (embedder_web_contents()->GetBrowserPluginGuest()) {
    // |point| is currently with respect to the top-most view (outermost
    // WebContents). We should subtract a displacement to find the point with
    // resepct to embedder's WebContents.
    point -= owner_rwhv->TransformPointToRootCoordSpace(gfx::Point())
                 .OffsetFromOrigin();
  }

  return point;
}

WebContentsImpl* BrowserPluginGuest::GetWebContents() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

gfx::Point BrowserPluginGuest::GetScreenCoordinates(
    const gfx::Point& relative_position) const {
  if (!attached())
    return relative_position;

  gfx::Point screen_pos(relative_position);
  screen_pos += frame_rect_.OffsetFromOrigin();
  return screen_pos;
}

void BrowserPluginGuest::SendMessageToEmbedder(
    std::unique_ptr<IPC::Message> msg) {
  // During tests, attache() may be true when there is no owner_web_contents_;
  // in this case just queue any messages we receive.
  if (!attached() || !owner_web_contents_) {
    // Some pages such as data URLs, javascript URLs, and about:blank
    // do not load external resources and so they load prior to attachment.
    // As a result, we must save all these IPCs until attachment and then
    // forward them so that the embedder gets a chance to see and process
    // the load events.
    pending_messages_.push_back(std::move(msg));
    return;
  }

  // If the guest is inside a cross-process frame, it is possible to get here
  // after the owner frame is detached. Then, the owner RenderWidgetHost will
  // be null and the message is dropped.
  if (auto* rwh = GetOwnerRenderWidgetHost())
    rwh->Send(msg.release());
}

void BrowserPluginGuest::DragSourceEndedAt(float client_x,
                                           float client_y,
                                           float screen_x,
                                           float screen_y,
                                           blink::WebDragOperation operation) {
  web_contents()->GetRenderViewHost()->GetWidget()->DragSourceEndedAt(
      gfx::PointF(client_x, client_y), gfx::PointF(screen_x, screen_y),
      operation);
  seen_embedder_drag_source_ended_at_ = true;
  EndSystemDragIfApplicable();
}

void BrowserPluginGuest::EndSystemDragIfApplicable() {
  // Ideally we'd want either WebDragStatusDrop or WebDragStatusLeave...
  // Call guest RVH->DragSourceSystemDragEnded() correctly on the guest where
  // the drag was initiated. Calling DragSourceSystemDragEnded() correctly
  // means we call it in all cases and also make sure we only call it once.
  // This ensures that the input state of the guest stays correct, otherwise
  // it will go stale and won't accept any further input events.
  //
  // The strategy used here to call DragSourceSystemDragEnded() on the RVH
  // is when the following conditions are met:
  //   a. Embedder has seen SystemDragEnded()
  //   b. Embedder has seen DragSourceEndedAt()
  //   c. The guest has seen some drag status update other than
  //      WebDragStatusUnknown. Note that this step should ideally be done
  //      differently: The guest has seen at least one of
  //      {WebDragStatusOver, WebDragStatusDrop}. However, if a user drags
  //      a source quickly outside of <webview> bounds, then the
  //      BrowserPluginGuest never sees any of these drag status updates,
  //      there we just check whether we've seen any drag status update or
  //      not.
  if (last_drag_status_ != blink::kWebDragStatusOver &&
      seen_embedder_drag_source_ended_at_ && seen_embedder_system_drag_ended_) {
    RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
        GetWebContents()->GetRenderViewHost());
    guest_rvh->GetWidget()->DragSourceSystemDragEnded();
    last_drag_status_ = blink::kWebDragStatusUnknown;
    seen_embedder_system_drag_ended_ = false;
    seen_embedder_drag_source_ended_at_ = false;
    ignore_dragged_url_ = true;
  }
}

void BrowserPluginGuest::EmbedderSystemDragEnded() {
  seen_embedder_system_drag_ended_ = true;
  EndSystemDragIfApplicable();
}

// TODO(wjmaclean): Replace this approach with ones based on std::function
// as in https://codereview.chromium.org/1404353004/ once all Chrome platforms
// support this. https://crbug.com/544212
std::unique_ptr<IPC::Message> BrowserPluginGuest::UpdateInstanceIdIfNecessary(
    std::unique_ptr<IPC::Message> msg) const {
  DCHECK(msg.get());

  int msg_browser_plugin_instance_id = browser_plugin::kInstanceIDNone;
  base::PickleIterator iter(*msg.get());
  if (!iter.ReadInt(&msg_browser_plugin_instance_id) ||
      msg_browser_plugin_instance_id != browser_plugin::kInstanceIDNone) {
    return msg;
  }

  // This method may be called with no browser_plugin_instance_id in tests.
  if (!browser_plugin_instance_id())
    return msg;

  std::unique_ptr<IPC::Message> new_msg(
      new IPC::Message(msg->routing_id(), msg->type(), msg->priority()));
  new_msg->WriteInt(browser_plugin_instance_id());

  // Copy remaining payload from original message.
  // TODO(wjmaclean): it would be nice if IPC::PickleIterator had a method
  // like 'RemainingBytes()' so that we don't have to include implementation-
  // specific details like sizeof() in the next line.
  DCHECK(msg->payload_size() >= sizeof(int));
  size_t remaining_bytes = msg->payload_size() - sizeof(int);
  // Some BrowserPluginMsgs only have the |browser_plugin_instance_id| and no
  // further payload. It they are enqueued, and require updating of the id, then
  // this would subsequently fail.
  // TODO(wjmaclean): It might be nice to enqueue the creation of the
  // IPC::Messages, rather than the messages themselves. Thus avoiding having to
  // perform custom read/writes.
  if (remaining_bytes) {
    const char* data = nullptr;
    bool read_success = iter.ReadBytes(&data, remaining_bytes);
    CHECK(read_success)
        << "Unexpected failure reading remaining IPC::Message payload.";
    new_msg->WriteBytes(data, remaining_bytes);
  }
  return new_msg;
}

void BrowserPluginGuest::SendQueuedMessages() {
  if (!attached())
    return;

  while (!pending_messages_.empty()) {
    std::unique_ptr<IPC::Message> message_ptr =
        std::move(pending_messages_.front());
    pending_messages_.pop_front();
    SendMessageToEmbedder(UpdateInstanceIdIfNecessary(std::move(message_ptr)));
  }
}

void BrowserPluginGuest::SendTextInputTypeChangedToView(
    RenderWidgetHostViewBase* guest_rwhv) {
  if (!guest_rwhv)
    return;

  if (!owner_web_contents_) {
    // If we were showing an interstitial, then we can end up here during
    // embedder shutdown or when the embedder navigates to a different page.
    // The call stack is roughly:
    // BrowserPluginGuest::SetFocus()
    // content::InterstitialPageImpl::Hide()
    // content::InterstitialPageImpl::DontProceed().
    //
    // TODO(lazyboy): Write a WebUI test once http://crbug.com/463674 is fixed.
    return;
  }

  if (last_text_input_state_.get()) {
    guest_rwhv->TextInputStateChanged(*last_text_input_state_);
    if (auto* rwh = guest_rwhv->host()) {
      // We need composition range information for some IMEs. To get the
      // updates, we need to explicitly ask the renderer to monitor and send the
      // composition information changes. RenderWidgetHostView of the page will
      // send the request to its process but the machinery for forwarding it to
      // BrowserPlugin is not there. Therefore, we send a direct request to the
      // guest process to start monitoring the state (see
      // https://crbug.com/714771).
      rwh->RequestCompositionUpdates(
          false, last_text_input_state_->type != ui::TEXT_INPUT_TYPE_NONE);
    }
  }
}

void BrowserPluginGuest::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted())
    RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.DidNavigate"));
}

void BrowserPluginGuest::RenderViewReady() {
  if (GuestMode::IsCrossProcessFrameGuest(GetWebContents()))
    return;

  RenderViewHost* rvh = GetWebContents()->GetRenderViewHost();
  // TODO(fsamuel): Investigate whether it's possible to update state earlier
  // here (see http://crbug.com/158151).
  RenderWidgetHostImpl::From(rvh->GetWidget())
      ->GetWidgetInputHandler()
      ->SetFocus(focused_);
  UpdateVisibility();

  // In case we've created a new guest render process after a crash, let the
  // associated BrowserPlugin know. We only need to send this if we're attached,
  // as guest_crashed_ is cleared automatically on attach anyways.
  if (attached()) {
    RenderWidgetHostViewGuest* rwhv = static_cast<RenderWidgetHostViewGuest*>(
        web_contents()->GetRenderWidgetHostView());
    if (rwhv) {
      SendMessageToEmbedder(std::make_unique<BrowserPluginMsg_GuestReady>(
          browser_plugin_instance_id(), rwhv->GetFrameSinkId()));
    }
  }

  RenderWidgetHostImpl::From(rvh->GetWidget())
      ->set_hung_renderer_delay(
          base::TimeDelta::FromMilliseconds(kHungRendererDelayMs));
}

void BrowserPluginGuest::RenderProcessGone(base::TerminationStatus status) {
  SendMessageToEmbedder(std::make_unique<BrowserPluginMsg_GuestGone>(
      browser_plugin_instance_id()));
  switch (status) {
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Killed"));
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Crashed"));
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      RecordAction(
          base::UserMetricsAction("BrowserPlugin.Guest.AbnormalDeath"));
      break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.LaunchFailed"));
      break;
    default:
      break;
  }
}

void BrowserPluginGuest::DidSetHasTouchEventHandlers(bool accept) {
  SendMessageToEmbedder(
      std::make_unique<BrowserPluginMsg_ShouldAcceptTouchEvents>(
          browser_plugin_instance_id(), accept));
}

void BrowserPluginGuest::DidTextInputStateChange(const TextInputState& params) {
  // Save the state of text input so we can restore it on focus.
  last_text_input_state_ = std::make_unique<TextInputState>(params);

  SendTextInputTypeChangedToView(static_cast<RenderWidgetHostViewBase*>(
      web_contents()->GetRenderWidgetHostView()));
}

void BrowserPluginGuest::DidLockMouse(bool user_gesture, bool privileged) {
  if (pending_lock_request_) {
    // Immediately reject the lock because only one pointerLock may be active
    // at a time.
    RenderWidgetHost* widget_host =
        web_contents()->GetRenderViewHost()->GetWidget();
    widget_host->Send(
        new WidgetMsg_LockMouse_ACK(widget_host->GetRoutingID(), false));
    return;
  }

  pending_lock_request_ = true;

  RenderWidgetHostImpl* owner = GetOwnerRenderWidgetHost();
  bool is_last_unlocked_by_target =
      owner ? owner->is_last_unlocked_by_target() : false;

  delegate_->RequestPointerLockPermission(
      user_gesture, is_last_unlocked_by_target,
      base::BindRepeating(&BrowserPluginGuest::PointerLockPermissionResponse,
                          weak_ptr_factory_.GetWeakPtr()));
}

void BrowserPluginGuest::DidUnlockMouse() {
  SendMessageToEmbedder(std::make_unique<BrowserPluginMsg_SetMouseLock>(
      browser_plugin_instance_id(), false));
}

// static
bool BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(
    const IPC::Message& message) {
  return (message.type() != BrowserPluginHostMsg_Attach::ID) &&
         (IPC_MESSAGE_CLASS(message) == BrowserPluginMsgStart);
}

bool BrowserPluginGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  // In --site-per-process, we do not need most of BrowserPluginGuest to drive
  // inner WebContents.
  // TODO(lazyboy): Fix this as part of http://crbug.com/330264. The required
  // parts of code from this class should be extracted to a separate class for
  // --site-per-process.
  if (GuestMode::IsCrossProcessFrameGuest(GetWebContents()))
    return false;

  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool BrowserPluginGuest::OnMessageReceived(const IPC::Message& message,
                                           RenderFrameHost* render_frame_host) {
  // This will eventually be the home for more IPC handlers that depend on
  // RenderFrameHost. Until more are moved here, though, the IPC_* macros won't
  // compile if there are no handlers for a platform. So we have both #if guards
  // around the whole thing (unfortunate but temporary), and #if guards where
  // they belong, only around the one IPC handler. TODO(avi): Move more of the
  // frame-based handlers to this function and remove the outer #if layer.
#if defined(OS_MACOSX)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(BrowserPluginGuest, message,
                                   render_frame_host)
    // MacOS X creates and populates platform-specific select drop-down menus
    // whereas other platforms merely create a popup window that the guest
    // renderer process paints inside.
    IPC_MESSAGE_HANDLER(FrameHostMsg_ShowPopup, OnShowPopup)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#else
  return false;
#endif
}

void BrowserPluginGuest::Attach(
    int browser_plugin_instance_id,
    WebContentsImpl* embedder_web_contents,
    const BrowserPluginHostMsg_Attach_Params& params) {
  browser_plugin_instance_id_ = browser_plugin_instance_id;
  // The guest is owned by the embedder. Attach is queued up so we cannot
  // change embedders before attach completes. If the embedder goes away,
  // so does the guest and so we will never call WillAttachComplete because
  // we have a weak ptr.
  delegate_->WillAttach(
      embedder_web_contents, browser_plugin_instance_id,
      params.is_full_page_plugin,
      base::BindOnce(&BrowserPluginGuest::OnWillAttachComplete,
                     weak_ptr_factory_.GetWeakPtr(), embedder_web_contents,
                     params));
}

void BrowserPluginGuest::OnWillAttachComplete(
    WebContentsImpl* embedder_web_contents,
    const BrowserPluginHostMsg_Attach_Params& params) {
  // If a RenderView has already been created for this new window, then we need
  // to initialize the browser-side state now so that the RenderFrameHostManager
  // does not create a new RenderView on navigation.
  if (has_render_view_) {
    // This will trigger a callback to RenderViewReady after a round-trip IPC.
    static_cast<RenderViewHostImpl*>(GetWebContents()->GetRenderViewHost())
        ->GetWidget()
        ->Init();
    GetWebContents()->GetMainFrame()->Init();
    WebContentsViewGuest* web_contents_view =
        static_cast<WebContentsViewGuest*>(GetWebContents()->GetView());
    if (!web_contents()->GetRenderViewHost()->GetWidget()->GetView()) {
      web_contents_view->CreateViewForWidget(
          web_contents()->GetRenderViewHost()->GetWidget(), true);
    }
  }

  InitInternal(params, embedder_web_contents);

  attached_ = true;
  SendQueuedMessages();

  delegate_->DidAttach(GetGuestRenderViewRoutingID());
  RenderWidgetHostViewGuest* rwhv = static_cast<RenderWidgetHostViewGuest*>(
      web_contents()->GetRenderWidgetHostView());
  if (rwhv)
    rwhv->OnAttached();
  has_render_view_ = true;

  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Attached"));
}

void BrowserPluginGuest::OnDetach(int browser_plugin_instance_id) {
  if (!attached())
    return;

  // It is important that the WebContents is notified before detaching.
  GetWebContents()->BrowserPluginGuestWillDetach();

  // This tells BrowserPluginGuest to queue up all IPCs to BrowserPlugin until
  // it's attached again.
  attached_ = false;

  RenderWidgetHostViewChildFrame* rwhv =
      static_cast<RenderWidgetHostViewChildFrame*>(
          web_contents()->GetRenderWidgetHostView());
  // If the guest is terminated, our host may already be gone.
  if (rwhv) {
    rwhv->UnregisterFrameSinkId();
    RenderWidgetHostViewBase* root_view =
        RenderWidgetHostViewGuest::GetRootView(rwhv);
    if (root_view)
      root_view->GetCursorManager()->ViewBeingDestroyed(rwhv);
  }

  delegate_->DidDetach();
}

void BrowserPluginGuest::OnDragStatusUpdate(int browser_plugin_instance_id,
                                            blink::WebDragStatus drag_status,
                                            const DropData& drop_data,
                                            blink::WebDragOperationsMask mask,
                                            const gfx::PointF& location) {
  RenderViewHost* host = GetWebContents()->GetRenderViewHost();
  auto* embedder = owner_web_contents_->GetBrowserPluginEmbedder();
  DropData filtered_data(drop_data);
  // TODO(paulmeyer): This will need to target the correct specific
  // RenderWidgetHost to work with OOPIFs. See crbug.com/647249.
  RenderWidgetHost* widget = host->GetWidget();
  widget->FilterDropData(&filtered_data);
  switch (drag_status) {
    case blink::kWebDragStatusEnter:
      widget->DragTargetDragEnter(filtered_data, location, location, mask,
                                  drop_data.key_modifiers);
      // Only track the URL being dragged over the guest if the link isn't
      // coming from the guest.
      if (!embedder->DragEnteredGuest(this))
        ignore_dragged_url_ = false;
      break;
    case blink::kWebDragStatusOver:
      widget->DragTargetDragOver(location, location, mask,
                                 drop_data.key_modifiers);
      break;
    case blink::kWebDragStatusLeave:
      embedder->DragLeftGuest(this);
      widget->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
      ignore_dragged_url_ = true;
      break;
    case blink::kWebDragStatusDrop:
      widget->DragTargetDrop(filtered_data, location, location,
                             drop_data.key_modifiers);

      if (!ignore_dragged_url_ && filtered_data.url.is_valid())
        delegate_->DidDropLink(filtered_data.url);
      ignore_dragged_url_ = true;
      break;
    case blink::kWebDragStatusUnknown:
      ignore_dragged_url_ = true;
      NOTREACHED();
  }
  last_drag_status_ = drag_status;
  EndSystemDragIfApplicable();
}

void BrowserPluginGuest::OnExecuteEditCommand(int browser_plugin_instance_id,
                                              const std::string& name) {
  RenderFrameHostImpl* focused_frame =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetFocusedFrame());
  if (!focused_frame || !focused_frame->GetFrameInputHandler())
    return;

  focused_frame->GetFrameInputHandler()->ExecuteEditCommand(name,
                                                            base::nullopt);
}

void BrowserPluginGuest::OnImeSetComposition(
    int browser_plugin_instance_id,
    const BrowserPluginHostMsg_SetComposition_Params& params) {
  std::vector<ui::ImeTextSpan> ui_ime_text_spans =
      ConvertBlinkImeTextSpansToUiImeTextSpans(params.ime_text_spans);
  GetWebContents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->GetWidgetInputHandler()
      ->ImeSetComposition(params.text, ui_ime_text_spans,
                          params.replacement_range, params.selection_start,
                          params.selection_end);
}

void BrowserPluginGuest::OnImeCommitText(
    int browser_plugin_instance_id,
    const base::string16& text,
    const std::vector<blink::WebImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
  std::vector<ui::ImeTextSpan> ui_ime_text_spans =
      ConvertBlinkImeTextSpansToUiImeTextSpans(ime_text_spans);
  GetWebContents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->GetWidgetInputHandler()
      ->ImeCommitText(text, ui_ime_text_spans, replacement_range,
                      relative_cursor_pos, base::OnceClosure());
}

void BrowserPluginGuest::OnImeFinishComposingText(
    int browser_plugin_instance_id,
    bool keep_selection) {
  DCHECK_EQ(browser_plugin_instance_id_, browser_plugin_instance_id);
  GetWebContents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->GetWidgetInputHandler()
      ->ImeFinishComposingText(keep_selection);
}

void BrowserPluginGuest::OnExtendSelectionAndDelete(
    int browser_plugin_instance_id,
    int before,
    int after) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      web_contents()->GetFocusedFrame());
  if (rfh && rfh->GetFrameInputHandler())
    rfh->GetFrameInputHandler()->ExtendSelectionAndDelete(before, after);
}

void BrowserPluginGuest::OnLockMouseAck(int browser_plugin_instance_id,
                                        bool succeeded) {
  RenderWidgetHost* widget_host =
      web_contents()->GetRenderViewHost()->GetWidget();
  widget_host->Send(
      new WidgetMsg_LockMouse_ACK(widget_host->GetRoutingID(), succeeded));
  pending_lock_request_ = false;
  if (succeeded)
    mouse_locked_ = true;
}

void BrowserPluginGuest::OnSetFocus(int browser_plugin_instance_id,
                                    bool focused,
                                    blink::WebFocusType focus_type) {
  RenderWidgetHostView* rwhv = web_contents()->GetRenderWidgetHostView();
  RenderWidgetHost* rwh = rwhv ? rwhv->GetRenderWidgetHost() : nullptr;
  SetFocus(rwh, focused, focus_type);
}

void BrowserPluginGuest::OnSetEditCommandsForNextKeyEvent(
    int browser_plugin_instance_id,
    const std::vector<EditCommand>& edit_commands) {
  GetWebContents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->GetWidgetInputHandler()
      ->SetEditCommandsForNextKeyEvent(edit_commands);
}

void BrowserPluginGuest::OnSetVisibility(int browser_plugin_instance_id,
                                         bool visible) {
  // For OOPIF-<webivew>, the remote frame will handle visibility state.
  if (GuestMode::IsCrossProcessFrameGuest(GetWebContents()))
    return;

  guest_visible_ = visible;

  // Do not use WebContents::UpdateWebContentsVisibility() because it ignores
  // visibility changes that come before the first change to VISIBLE.
  if (!guest_visible_ || embedder_visibility_ == Visibility::HIDDEN)
    GetWebContents()->WasHidden();
  else if (embedder_visibility_ == Visibility::VISIBLE)
    GetWebContents()->WasShown();
  else
    GetWebContents()->WasOccluded();
}

void BrowserPluginGuest::OnUnlockMouseAck(int browser_plugin_instance_id) {
  // mouse_locked_ could be false here if the lock attempt was cancelled due
  // to window focus, or for various other reasons before the guest was informed
  // of the lock's success.
  if (mouse_locked_) {
    RenderWidgetHost* widget_host =
        web_contents()->GetRenderViewHost()->GetWidget();
    widget_host->Send(new WidgetMsg_MouseLockLost(widget_host->GetRoutingID()));
  }
  mouse_locked_ = false;
}

void BrowserPluginGuest::OnSynchronizeVisualProperties(
    int browser_plugin_instance_id,
    const FrameVisualProperties& visual_properties) {
  if ((local_surface_id_allocation_.local_surface_id() >
       visual_properties.local_surface_id_allocation.local_surface_id()) ||
      ((frame_rect_.size() != visual_properties.screen_space_rect.size() ||
        screen_info_ != visual_properties.screen_info ||
        capture_sequence_number_ != visual_properties.capture_sequence_number ||
        zoom_level_ != visual_properties.zoom_level) &&
       local_surface_id_allocation_.local_surface_id() ==
           visual_properties.local_surface_id_allocation.local_surface_id())) {
    SiteInstance* owner_site_instance = delegate_->GetOwnerSiteInstance();
    bad_message::ReceivedBadMessage(
        owner_site_instance->GetProcess(),
        bad_message::BPG_RESIZE_PARAMS_CHANGED_LOCAL_SURFACE_ID_UNCHANGED);
    return;
  }

  screen_info_ = visual_properties.screen_info;
  frame_rect_ = visual_properties.screen_space_rect;
  zoom_level_ = visual_properties.zoom_level;

  GetWebContents()->SendScreenRects();
  local_surface_id_allocation_ = visual_properties.local_surface_id_allocation;
  bool capture_sequence_number_changed =
      capture_sequence_number_ != visual_properties.capture_sequence_number;
  capture_sequence_number_ = visual_properties.capture_sequence_number;

  RenderWidgetHostView* view = web_contents()->GetRenderWidgetHostView();
  if (!view)
    return;

  // We could add functionality to set a specific capture sequence number on the
  // |view|, but knowing that it's changed is sufficient for us simply request
  // that our RenderWidgetHostView synchronizes its surfaces. Note that this
  // should only happen during web tests, since that is the only call that
  // should trigger the capture sequence number to change.
  if (capture_sequence_number_changed)
    view->EnsureSurfaceSynchronizedForWebTest();

  RenderWidgetHostImpl* render_widget_host =
      RenderWidgetHostImpl::From(view->GetRenderWidgetHost());
  DCHECK(render_widget_host);

  render_widget_host->SetAutoResize(visual_properties.auto_resize_enabled,
                                    visual_properties.min_size_for_auto_resize,
                                    visual_properties.max_size_for_auto_resize);

  render_widget_host->SynchronizeVisualProperties();
}

#if defined(OS_MACOSX)
void BrowserPluginGuest::OnShowPopup(
    RenderFrameHost* render_frame_host,
    const FrameHostMsg_ShowPopup_Params& params) {
  gfx::Rect translated_bounds(params.bounds);
  WebContents* guest = web_contents();
  if (GuestMode::IsCrossProcessFrameGuest(guest)) {
    translated_bounds.set_origin(
        guest->GetRenderWidgetHostView()->TransformPointToRootCoordSpace(
            translated_bounds.origin()));
  } else {
    translated_bounds.Offset(frame_rect_.OffsetFromOrigin());
  }
  BrowserPluginPopupMenuHelper popup_menu_helper(
      owner_web_contents_->GetMainFrame(), render_frame_host);
  popup_menu_helper.ShowPopupMenu(translated_bounds,
                                  params.item_height,
                                  params.item_font_size,
                                  params.selected_item,
                                  params.popup_items,
                                  params.right_aligned,
                                  params.allow_multiple_selection);
}
#endif

void BrowserPluginGuest::OnShowWidget(int widget_route_id,
                                      const gfx::Rect& initial_rect) {
  int process_id = GetWebContents()->GetMainFrame()->GetProcess()->GetID();
  GetWebContents()->ShowCreatedWidget(process_id, widget_route_id,
                                      initial_rect);
}

void BrowserPluginGuest::OnTakeFocus(bool reverse) {
  SendMessageToEmbedder(std::make_unique<BrowserPluginMsg_AdvanceFocus>(
      browser_plugin_instance_id(), reverse));
}

int BrowserPluginGuest::GetGuestRenderViewRoutingID() {
  if (GuestMode::IsCrossProcessFrameGuest(GetWebContents())) {
    // We don't use the proxy to send postMessage in --site-per-process, since
    // we use the contentWindow directly from the frame element instead.
    return MSG_ROUTING_NONE;
  }

  if (guest_render_view_routing_id_ != MSG_ROUTING_NONE)
    return guest_render_view_routing_id_;

  // In order to enable the embedder to post messages to the
  // guest, we need to create a RenderFrameProxyHost in root node of guest
  // WebContents' frame tree (i.e., create a RenderFrameProxy in the embedder
  // process which can be used by the embedder to post messages to the guest).
  // The creation of RFPH for the reverse path, which enables the guest to post
  // messages to the embedder, will be postponed to when the embedder posts its
  // first message to the guest.
  //
  // TODO(fsamuel): Make sure this works for transferring guests across
  // owners in different processes. We probably need to clear the
  // |guest_render_view_routing_id_| and perform any necessary cleanup on Detach
  // to enable this.
  //
  // TODO(ekaramad): If the guest is embedded inside a cross-process <iframe>
  // (e.g., <embed>-ed PDF), the reverse proxy will not be created and the
  // posted message's source attribute will be null which in turn breaks the
  // two-way messaging between the guest and the embedder. We should either
  // create a RenderFrameProxyHost for the reverse path, or implement
  // MimeHandlerViewGuest using OOPIF (https://crbug.com/659750).
  SiteInstance* owner_site_instance = delegate_->GetOwnerSiteInstance();
  if (!owner_site_instance)
    return MSG_ROUTING_NONE;

  RenderFrameHostManager* rfh_manager =
      GetWebContents()->GetFrameTree()->root()->render_manager();
  rfh_manager->CreateRenderFrameProxy(owner_site_instance);
  guest_render_view_routing_id_ =
      rfh_manager->GetRenderFrameProxyHost(owner_site_instance)
          ->GetRenderViewHost()
          ->GetRoutingID();

  return guest_render_view_routing_id_;
}

}  // namespace content
