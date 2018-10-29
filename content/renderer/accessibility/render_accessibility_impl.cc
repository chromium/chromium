// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/accessibility_messages.h"
#include "content/renderer/accessibility/blink_ax_enum_conversion.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

using blink::WebAXContext;
using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFloatRect;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebPoint;
using blink::WebRect;
using blink::WebSettings;
using blink::WebView;

namespace {
// The next token to use to distinguish between ack events sent to this
// RenderAccessibilityImpl and a previous instance.
static int g_next_ack_token = 1;
}

namespace content {

// Cap the number of nodes returned in an accessibility
// tree snapshot to avoid outrageous memory or bandwidth
// usage.
const size_t kMaxSnapshotNodeCount = 5000;

// static
void RenderAccessibilityImpl::SnapshotAccessibilityTree(
    RenderFrameImpl* render_frame,
    AXContentTreeUpdate* response,
    ui::AXMode ax_mode) {
  TRACE_EVENT0("accessibility",
               "RenderAccessibilityImpl::SnapshotAccessibilityTree");

  DCHECK(render_frame);
  DCHECK(response);
  if (!render_frame->GetWebFrame())
    return;

  WebDocument document = render_frame->GetWebFrame()->GetDocument();
  WebAXContext context(document);
  WebAXObject root = context.Root();
  if (!root.UpdateLayoutAndCheckValidity())
    return;
  BlinkAXTreeSource tree_source(render_frame, ax_mode);
  tree_source.SetRoot(root);
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source);
  BlinkAXTreeSerializer serializer(&tree_source);
  serializer.set_max_node_count(kMaxSnapshotNodeCount);

  if (serializer.SerializeChanges(context.Root(), response))
    return;

  // It's possible for the page to fail to serialize the first time due to
  // aria-owns rearranging the page while it's being scanned. Try a second
  // time.
  *response = AXContentTreeUpdate();
  if (serializer.SerializeChanges(context.Root(), response))
    return;

  // It failed again. Clear the response object because it might have errors.
  *response = AXContentTreeUpdate();
  LOG(WARNING) << "Unable to serialize accessibility tree.";
}

RenderAccessibilityImpl::RenderAccessibilityImpl(RenderFrameImpl* render_frame,
                                                 ui::AXMode mode)
    : RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      tree_source_(render_frame, mode),
      serializer_(&tree_source_),
      plugin_tree_source_(nullptr),
      last_scroll_offset_(gfx::Size()),
      ack_pending_(false),
      reset_token_(0),
      during_action_(false),
      weak_factory_(this) {
  ack_token_ = g_next_ack_token++;
  WebView* web_view = render_frame_->GetRenderView()->GetWebView();
  WebSettings* settings = web_view->GetSettings();

#if defined(OS_ANDROID)
  // Password values are only passed through on Android.
  settings->SetAccessibilityPasswordValuesEnabled(true);
#endif

#if !defined(OS_ANDROID)
  // Inline text boxes can be enabled globally on all except Android.
  // On Android they can be requested for just a specific node.
  if (mode.has_mode(ui::AXMode::kInlineTextBoxes))
    settings->SetInlineTextBoxAccessibilityEnabled(true);
#endif

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    ax_context_.reset(new blink::WebAXContext(document));

    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by sending it a notification.
    HandleAXEvent(WebAXObject::FromWebDocument(document),
                  ax::mojom::Event::kLayoutComplete);
  }
}

RenderAccessibilityImpl::~RenderAccessibilityImpl() {
}

void RenderAccessibilityImpl::DidCreateNewDocument() {
  ax_context_.reset(new blink::WebAXContext(GetMainDocument()));
}

void RenderAccessibilityImpl::AccessibilityModeChanged() {
  ui::AXMode new_mode = render_frame_->accessibility_mode();
  if (tree_source_.accessibility_mode() == new_mode)
    return;
  tree_source_.SetAccessibilityMode(new_mode);

#if !defined(OS_ANDROID)
  // Inline text boxes can be enabled globally on all except Android.
  // On Android they can be requested for just a specific node.
  RenderView* render_view = render_frame_->GetRenderView();
  if (render_view) {
    WebView* web_view = render_view->GetWebView();
    if (web_view) {
      WebSettings* settings = web_view->GetSettings();
      if (settings) {
        if (new_mode.has_mode(ui::AXMode::kInlineTextBoxes)) {
          settings->SetInlineTextBoxAccessibilityEnabled(true);
          tree_source_.GetRoot().LoadInlineTextBoxes();
        } else {
          settings->SetInlineTextBoxAccessibilityEnabled(false);
        }
      }
    }
  }
#endif  // !defined(OS_ANDROID)

  serializer_.Reset();
  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    // If there are any events in flight, |HandleAXEvent| will refuse to process
    // our new event.
    pending_events_.clear();
    auto webax_object = WebAXObject::FromWebDocument(document);
    ax::mojom::Event event = webax_object.IsLoaded()
                                 ? ax::mojom::Event::kLoadComplete
                                 : ax::mojom::Event::kLayoutComplete;
    HandleAXEvent(webax_object, event);
  }
}

bool RenderAccessibilityImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  during_action_ = true;
  IPC_BEGIN_MESSAGE_MAP(RenderAccessibilityImpl, message)

    IPC_MESSAGE_HANDLER(AccessibilityMsg_PerformAction, OnPerformAction)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_EventBundle_ACK, OnEventsAck)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_HitTest, OnHitTest)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_FatalError, OnFatalError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  during_action_ = false;
  return handled;
}

void RenderAccessibilityImpl::HandleWebAccessibilityEvent(
    const blink::WebAXObject& obj,
    ax::mojom::Event event) {
  HandleAXEvent(obj, event);
}

void RenderAccessibilityImpl::MarkWebAXObjectDirty(
    const blink::WebAXObject& obj,
    bool subtree) {
  DirtyObject dirty_object;
  dirty_object.obj = obj;
  dirty_object.event_from = GetEventFrom();
  dirty_objects_.push_back(dirty_object);

  if (subtree)
    serializer_.InvalidateSubtree(obj);

  ScheduleSendAccessibilityEventsIfNeeded();
}

void RenderAccessibilityImpl::HandleAccessibilityFindInPageResult(
    int identifier,
    int match_index,
    const blink::WebAXObject& start_object,
    int start_offset,
    const blink::WebAXObject& end_object,
    int end_offset) {
  AccessibilityHostMsg_FindInPageResultParams params;
  params.request_id = identifier;
  params.match_index = match_index;
  params.start_id = start_object.AxID();
  params.start_offset = start_offset;
  params.end_id = end_object.AxID();
  params.end_offset = end_offset;
  Send(new AccessibilityHostMsg_FindInPageResult(routing_id(), params));
}

void RenderAccessibilityImpl::AccessibilityFocusedNodeChanged(
    const WebNode& node) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  if (node.IsNull()) {
    // When focus is cleared, implicitly focus the document.
    // TODO(dmazzoni): Make Blink send this notification instead.
    HandleAXEvent(WebAXObject::FromWebDocument(document),
                  ax::mojom::Event::kBlur);
  }
}

void RenderAccessibilityImpl::HandleAXEvent(const blink::WebAXObject& obj,
                                            ax::mojom::Event event,
                                            int action_request_id) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  if (document.GetFrame()) {
    gfx::Size scroll_offset = document.GetFrame()->GetScrollOffset();
    if (scroll_offset != last_scroll_offset_) {
      // Make sure the browser is always aware of the scroll position of
      // the root document element by posting a generic notification that
      // will update it.
      // TODO(dmazzoni): remove this as soon as
      // https://bugs.webkit.org/show_bug.cgi?id=73460 is fixed.
      last_scroll_offset_ = scroll_offset;
      auto webax_object = WebAXObject::FromWebDocument(document);
      if (!obj.Equals(webax_object)) {
        HandleAXEvent(webax_object, ax::mojom::Event::kLayoutComplete);
      }
    }
  }

#if defined(OS_ANDROID)
  // Force the newly focused node to be re-serialized so we include its
  // inline text boxes.
  if (event == ax::mojom::Event::kFocus)
    serializer_.InvalidateSubtree(obj);
#endif

  // If some cell IDs have been added or removed, we need to update the whole
  // table.
  if (obj.Role() == ax::mojom::Role::kRow &&
      event == ax::mojom::Event::kChildrenChanged) {
    WebAXObject table_like_object = obj.ParentObject();
    if (!table_like_object.IsDetached()) {
      serializer_.InvalidateSubtree(table_like_object);
      HandleAXEvent(table_like_object, ax::mojom::Event::kChildrenChanged);
    }
  }

  // If a select tag is opened or closed, all the children must be updated
  // because their visibility may have changed.
  if (obj.Role() == ax::mojom::Role::kMenuListPopup &&
      event == ax::mojom::Event::kChildrenChanged) {
    WebAXObject popup_like_object = obj.ParentObject();
    if (!popup_like_object.IsDetached()) {
      serializer_.InvalidateSubtree(popup_like_object);
      HandleAXEvent(popup_like_object, ax::mojom::Event::kChildrenChanged);
    }
  }

  // Add the accessibility object to our cache and ensure it's valid.
  ui::AXEvent acc_event;
  acc_event.id = obj.AxID();
  acc_event.event_type = event;
  acc_event.event_from = GetEventFrom();
  acc_event.action_request_id = action_request_id;

  // Discard duplicate accessibility events.
  for (uint32_t i = 0; i < pending_events_.size(); ++i) {
    if (pending_events_[i].id == acc_event.id &&
        pending_events_[i].event_type == acc_event.event_type) {
      return;
    }
  }
  pending_events_.push_back(acc_event);

  ScheduleSendAccessibilityEventsIfNeeded();
}

void RenderAccessibilityImpl::ScheduleSendAccessibilityEventsIfNeeded() {
  // Don't send accessibility events for frames that are not in the frame tree
  // yet (i.e., provisional frames used for remote-to-local navigations, which
  // haven't committed yet).  Doing so might trigger layout, which may not work
  // correctly for those frames.  The events should be sent once such a frame
  // commits.
  if (!render_frame_->in_frame_tree())
    return;

  if (!ack_pending_ && !weak_factory_.HasWeakPtrs()) {
    // When no accessibility events are in-flight post a task to send
    // the events to the browser. We use PostTask so that we can queue
    // up additional events.
    render_frame_->GetTaskRunner(blink::TaskType::kInternalDefault)
        ->PostTask(FROM_HERE,
                   base::BindOnce(
                       &RenderAccessibilityImpl::SendPendingAccessibilityEvents,
                       weak_factory_.GetWeakPtr()));
  }
}

ax::mojom::EventFrom RenderAccessibilityImpl::GetEventFrom() {
  if (blink::WebUserGestureIndicator::IsProcessingUserGesture(
          render_frame_->GetWebFrame())) {
    return ax::mojom::EventFrom::kUser;
  }

  if (during_action_)
    return ax::mojom::EventFrom::kAction;

  return ax::mojom::EventFrom::kPage;
}

int RenderAccessibilityImpl::GenerateAXID() {
  WebAXObject root = tree_source_.GetRoot();
  return root.GenerateAXID();
}

void RenderAccessibilityImpl::SetPluginTreeSource(
    RenderAccessibilityImpl::PluginAXTreeSource* plugin_tree_source) {
  plugin_tree_source_ = plugin_tree_source;
  plugin_serializer_.reset(new PluginAXTreeSerializer(plugin_tree_source_));

  OnPluginRootNodeUpdated();
}

void RenderAccessibilityImpl::OnPluginRootNodeUpdated() {
  // Search the accessibility tree for an EMBED element and post a
  // children changed notification on it to force it to update the
  // plugin accessibility tree.

  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  WebAXObject root = tree_source_.GetRoot();
  if (!root.UpdateLayoutAndCheckValidity())
    return;

  base::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(root);
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    WebNode node = obj.GetNode();
    if (!node.IsNull() && node.IsElementNode()) {
      WebElement element = node.To<WebElement>();
      if (element.HasHTMLTagName("embed")) {
        HandleAXEvent(obj, ax::mojom::Event::kChildrenChanged);
        break;
      }
    }

    // Explore children of this object.
    std::vector<blink::WebAXObject> children;
    tree_source_.GetChildren(obj, &children);
    for (size_t i = 0; i < children.size(); ++i)
      objs_to_explore.push(children[i]);
  }
}

WebDocument RenderAccessibilityImpl::GetMainDocument() {
  if (render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->GetDocument();
  return WebDocument();
}

void RenderAccessibilityImpl::SendPendingAccessibilityEvents() {
  TRACE_EVENT0("accessibility",
               "RenderAccessibilityImpl::SendPendingAccessibilityEvents");

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  if (pending_events_.empty())
    return;

  ack_pending_ = true;

  // Make a copy of the events, because it's possible that
  // actions inside this loop will cause more events to be
  // queued up.
  std::vector<ui::AXEvent> src_events = pending_events_;
  pending_events_.clear();

  // The serialized event bundle to send to the browser.
  AccessibilityHostMsg_EventBundleParams bundle;

  // Keep track of nodes in the tree that need to be updated.
  std::vector<DirtyObject> dirty_objects = dirty_objects_;
  dirty_objects_.clear();

  // If there's a layout complete message, we need to send location changes.
  bool had_layout_complete_messages = false;

  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);

  // Loop over each event and generate an updated event message.
  for (size_t i = 0; i < src_events.size(); ++i) {
    ui::AXEvent& event = src_events[i];
    if (event.event_type == ax::mojom::Event::kLayoutComplete)
      had_layout_complete_messages = true;

    auto obj = WebAXObject::FromWebDocumentByID(document, event.id);

    // Make sure the object still exists.
    if (!obj.UpdateLayoutAndCheckValidity())
      continue;

    // If it's ignored, find the first ancestor that's not ignored.
    while (!obj.IsDetached() && obj.AccessibilityIsIgnored())
      obj = obj.ParentObject();

    // Make sure it's a descendant of our root node - exceptions include the
    // scroll area that's the parent of the main document (we ignore it), and
    // possibly nodes attached to a different document.
    if (!tree_source_.IsInTree(obj))
      continue;

    bundle.events.push_back(event);

    // Whenever there's a change within a table, invalidate the
    // whole table so that row and cell indexes are recomputed.
    const ax::mojom::Role role = obj.Role();
    if (ui::IsTableLike(role) || role == ax::mojom::Role::kRow ||
        ui::IsCellOrTableHeader(role)) {
      auto table = obj;
      while (!table.IsDetached() && !ui::IsTableLike(table.Role()))
        table = table.ParentObject();
      if (!table.IsDetached())
        serializer_.InvalidateSubtree(table);
    }

    VLOG(1) << "Accessibility event: " << ui::ToString(event.event_type)
            << " on node id " << event.id;

    DirtyObject dirty_object;
    dirty_object.obj = obj;
    dirty_object.event_from = event.event_from;
    dirty_objects.push_back(dirty_object);
  }

  // Now serialize all dirty objects. Keep track of IDs serialized
  // so we don't have to serialize the same node twice.
  std::set<int32_t> already_serialized_ids;
  for (size_t i = 0; i < dirty_objects.size(); i++) {
    auto obj = dirty_objects[i].obj;
    if (already_serialized_ids.find(obj.AxID()) != already_serialized_ids.end())
      continue;

    AXContentTreeUpdate update;
    update.event_from = dirty_objects[i].event_from;
    // If there's a plugin, force the tree data to be generated in every
    // message so the plugin can merge its own tree data changes.
    if (plugin_tree_source_)
      update.has_tree_data = true;

    if (!serializer_.SerializeChanges(obj, &update)) {
      VLOG(1) << "Failed to serialize one accessibility event.";
      continue;
    }

    if (plugin_tree_source_)
      AddPluginTreeToUpdate(&update);

    // For each node in the update, set the location in our map from
    // ids to locations.
    for (size_t j = 0; j < update.nodes.size(); ++j) {
      ui::AXNodeData& src = update.nodes[j];
      ui::AXRelativeBounds& dst = locations_[update.nodes[j].id];
      dst.offset_container_id = src.offset_container_id;
      dst.bounds = src.location;
      dst.transform.reset(nullptr);
      if (src.transform)
        dst.transform.reset(new gfx::Transform(*src.transform));
    }

    for (size_t j = 0; j < update.nodes.size(); ++j)
      already_serialized_ids.insert(update.nodes[j].id);

    bundle.updates.push_back(update);

    VLOG(1) << "Accessibility tree update:\n" << update.ToString();
  }

  Send(new AccessibilityHostMsg_EventBundle(routing_id(), bundle, reset_token_,
                                            ack_token_));
  reset_token_ = 0;

  if (had_layout_complete_messages)
    SendLocationChanges();
}

void RenderAccessibilityImpl::SendLocationChanges() {
  TRACE_EVENT0("accessibility", "RenderAccessibilityImpl::SendLocationChanges");

  std::vector<AccessibilityHostMsg_LocationChangeParams> messages;

  // Update layout on the root of the tree.
  WebAXObject root = tree_source_.GetRoot();
  if (!root.UpdateLayoutAndCheckValidity())
    return;

  // Do a breadth-first explore of the whole blink AX tree.
  base::hash_map<int, ui::AXRelativeBounds> new_locations;
  base::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(root);
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    // See if we had a previous location. If not, this whole subtree must
    // be new, so don't continue to explore this branch.
    int id = obj.AxID();
    auto iter = locations_.find(id);
    if (iter == locations_.end())
      continue;

    // If the location has changed, append it to the IPC message.
    WebAXObject offset_container;
    WebFloatRect bounds_in_container;
    SkMatrix44 container_transform;
    obj.GetRelativeBounds(offset_container, bounds_in_container,
                          container_transform);
    ui::AXRelativeBounds new_location;
    new_location.offset_container_id = offset_container.AxID();
    new_location.bounds = bounds_in_container;
    if (!container_transform.isIdentity())
      new_location.transform = base::WrapUnique(
          new gfx::Transform(container_transform));
    if (iter->second != new_location) {
      AccessibilityHostMsg_LocationChangeParams message;
      message.id = id;
      message.new_location = new_location;
      messages.push_back(message);
    }

    // Save the new location.
    new_locations[id] = new_location;

    // Explore children of this object.
    std::vector<blink::WebAXObject> children;
    tree_source_.GetChildren(obj, &children);
    for (size_t i = 0; i < children.size(); ++i)
      objs_to_explore.push(children[i]);
  }
  locations_.swap(new_locations);

  Send(new AccessibilityHostMsg_LocationChanges(routing_id(), messages));
}

void RenderAccessibilityImpl::OnPerformAction(
    const ui::AXActionData& data) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  auto root = WebAXObject::FromWebDocument(document);
  if (!root.UpdateLayoutAndCheckValidity())
    return;

  auto target = WebAXObject::FromWebDocumentByID(document, data.target_node_id);
  auto anchor = WebAXObject::FromWebDocumentByID(document, data.anchor_node_id);
  auto focus = WebAXObject::FromWebDocumentByID(document, data.focus_node_id);

  switch (data.action) {
    case ax::mojom::Action::kBlur:
      root.Focus();
      break;
    case ax::mojom::Action::kClearAccessibilityFocus:
      target.ClearAccessibilityFocus();
      break;
    case ax::mojom::Action::kDecrement:
      target.Decrement();
      break;
    case ax::mojom::Action::kDoDefault:
      target.Click();
      break;
    case ax::mojom::Action::kGetImageData:
      OnGetImageData(target, data.target_rect.size());
      break;
    case ax::mojom::Action::kHitTest:
      DCHECK(data.hit_test_event_to_fire != ax::mojom::Event::kNone);
      OnHitTest(data.target_point, data.hit_test_event_to_fire,
                data.request_id);
      break;
    case ax::mojom::Action::kIncrement:
      target.Increment();
      break;
    case ax::mojom::Action::kScrollToMakeVisible:
      target.ScrollToMakeVisibleWithSubFocus(
          WebRect(data.target_rect.x(), data.target_rect.y(),
                  data.target_rect.width(), data.target_rect.height()));
      break;
    case ax::mojom::Action::kScrollToPoint:
      target.ScrollToGlobalPoint(
          WebPoint(data.target_point.x(), data.target_point.y()));
      break;
    case ax::mojom::Action::kLoadInlineTextBoxes:
      OnLoadInlineTextBoxes(target);
      break;
    case ax::mojom::Action::kFocus:
      target.Focus();
      break;
    case ax::mojom::Action::kSetAccessibilityFocus:
      target.SetAccessibilityFocus();
      break;
    case ax::mojom::Action::kSetScrollOffset:
      target.SetScrollOffset(
          WebPoint(data.target_point.x(), data.target_point.y()));
      break;
    case ax::mojom::Action::kSetSelection:
      anchor.SetSelection(anchor, data.anchor_offset, focus, data.focus_offset);
      HandleAXEvent(root, ax::mojom::Event::kLayoutComplete);
      break;
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
      target.SetSequentialFocusNavigationStartingPoint();
      break;
    case ax::mojom::Action::kSetValue:
      target.SetValue(blink::WebString::FromUTF8(data.value));
      HandleAXEvent(target, ax::mojom::Event::kValueChanged);
      break;
    case ax::mojom::Action::kShowContextMenu:
      target.ShowContextMenu();
      break;
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollUp:
    case ax::mojom::Action::kScrollDown:
    case ax::mojom::Action::kScrollLeft:
    case ax::mojom::Action::kScrollRight:
      Scroll(target, data.action);
      break;
    case ax::mojom::Action::kCustomAction:
    case ax::mojom::Action::kReplaceSelectedText:
    case ax::mojom::Action::kNone:
      NOTREACHED();
      break;
  }
}

void RenderAccessibilityImpl::OnEventsAck(int ack_token) {
  // Ignore acks intended for a different or previous instance.
  if (ack_token_ != ack_token)
    return;

  DCHECK(ack_pending_);
  ack_pending_ = false;
  SendPendingAccessibilityEvents();
}

void RenderAccessibilityImpl::OnFatalError() {
  CHECK(false) << "Invalid accessibility tree.";
}

void RenderAccessibilityImpl::OnHitTest(const gfx::Point& point,
                                        ax::mojom::Event event_to_fire,
                                        int action_request_id) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;
  auto root_obj = WebAXObject::FromWebDocument(document);
  if (!root_obj.UpdateLayoutAndCheckValidity())
    return;

  WebAXObject obj = root_obj.HitTest(point);
  if (obj.IsDetached())
    return;

  // If the object that was hit has a child frame, we have to send a
  // message back to the browser to do the hit test in the child frame,
  // recursively.
  AXContentNodeData data;
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  tree_source_.SerializeNode(obj, &data);
  if (data.HasContentIntAttribute(AX_CONTENT_ATTR_CHILD_ROUTING_ID) ||
      data.HasContentIntAttribute(
          AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID)) {
    Send(new AccessibilityHostMsg_ChildFrameHitTestResult(
        routing_id(), action_request_id, point,
        data.GetContentIntAttribute(AX_CONTENT_ATTR_CHILD_ROUTING_ID),
        data.GetContentIntAttribute(
            AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID),
        event_to_fire));
    return;
  }

  // Otherwise, send an event on the node that was hit.
  HandleAXEvent(obj, event_to_fire, action_request_id);
}

void RenderAccessibilityImpl::OnLoadInlineTextBoxes(
    const blink::WebAXObject& obj) {
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  if (tree_source_.ShouldLoadInlineTextBoxes(obj))
    return;

  tree_source_.SetLoadInlineTextBoxesForId(obj.AxID());

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  // This object may not be a leaf node. Force the whole subtree to be
  // re-serialized.
  serializer_.InvalidateSubtree(obj);

  // Explicitly send a tree change update event now.
  HandleAXEvent(obj, ax::mojom::Event::kTreeChanged);
}

void RenderAccessibilityImpl::OnGetImageData(
    const blink::WebAXObject& obj, const gfx::Size& max_size) {
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  if (tree_source_.image_data_node_id() == obj.AxID())
    return;

  tree_source_.set_image_data_node_id(obj.AxID());
  tree_source_.set_max_image_data_size(max_size);

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  serializer_.InvalidateSubtree(obj);
  HandleAXEvent(obj, ax::mojom::Event::kImageFrameUpdated);
}

void RenderAccessibilityImpl::OnReset(int reset_token) {
  reset_token_ = reset_token;
  serializer_.Reset();
  pending_events_.clear();

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    // Tree-only mode gets used by the automation extension API which requires a
    // load complete event to invoke listener callbacks.
    auto webax_object = WebAXObject::FromWebDocument(document);
    ax::mojom::Event evt = webax_object.IsLoaded()
                               ? ax::mojom::Event::kLoadComplete
                               : ax::mojom::Event::kLayoutComplete;
    HandleAXEvent(webax_object, evt);
  }
}

void RenderAccessibilityImpl::OnDestruct() {
  delete this;
}

void RenderAccessibilityImpl::AddPluginTreeToUpdate(
    AXContentTreeUpdate* update) {
  for (size_t i = 0; i < update->nodes.size(); ++i) {
    if (update->nodes[i].role == ax::mojom::Role::kEmbeddedObject) {
      const ui::AXNode* root = plugin_tree_source_->GetRoot();
      update->nodes[i].child_ids.push_back(root->id());

      ui::AXTreeUpdate plugin_update;
      plugin_serializer_->SerializeChanges(root, &plugin_update);

      // We have to copy the updated nodes using a loop because we're
      // converting from a generic ui::AXNodeData to a vector of its
      // content-specific subclass AXContentNodeData.
      size_t old_count = update->nodes.size();
      size_t new_count = plugin_update.nodes.size();
      update->nodes.resize(old_count + new_count);
      for (size_t j = 0; j < new_count; ++j)
        update->nodes[old_count + j] = plugin_update.nodes[j];
      break;
    }
  }

  if (plugin_tree_source_->GetTreeData(&update->tree_data))
    update->has_tree_data = true;
}

void RenderAccessibilityImpl::Scroll(const WebAXObject& target,
                                     ax::mojom::Action scroll_action) {
  WebAXObject offset_container;
  WebFloatRect bounds;
  SkMatrix44 container_transform;
  target.GetRelativeBounds(offset_container, bounds, container_transform);

  if (bounds.IsEmpty())
    return;

  WebPoint initial = target.GetScrollOffset();
  WebPoint min = target.MinimumScrollOffset();
  WebPoint max = target.MaximumScrollOffset();

  // TODO(zhelfins): This 4/5ths came from the Android implementation, revisit
  // to find the appropriate modifier to keep enough context onscreen after
  // scrolling.
  int page_x = std::max((int)(bounds.width * 4 / 5), 1);
  int page_y = std::max((int)(bounds.height * 4 / 5), 1);

  // Forward/backward defaults to down/up unless it can only be scrolled
  // horizontally.
  if (scroll_action == ax::mojom::Action::kScrollForward)
    scroll_action = max.y > min.y ? ax::mojom::Action::kScrollDown
                                  : ax::mojom::Action::kScrollRight;
  if (scroll_action == ax::mojom::Action::kScrollBackward)
    scroll_action = max.y > min.y ? ax::mojom::Action::kScrollUp
                                  : ax::mojom::Action::kScrollLeft;

  int x = initial.x;
  int y = initial.y;
  switch (scroll_action) {
    case ax::mojom::Action::kScrollUp:
      if (initial.y == min.y)
        return;
      y = std::max(initial.y - page_y, min.y);
      break;
    case ax::mojom::Action::kScrollDown:
      if (initial.y == max.y)
        return;
      y = std::min(initial.y + page_y, max.y);
      break;
    case ax::mojom::Action::kScrollLeft:
      if (initial.x == min.x)
        return;
      x = std::max(initial.x - page_x, min.x);
      break;
    case ax::mojom::Action::kScrollRight:
      if (initial.x == max.x)
        return;
      x = std::min(initial.x + page_x, max.x);
      break;
    default:
      NOTREACHED();
  }

  target.SetScrollOffset(WebPoint(x, y));
}

void RenderAccessibilityImpl::ScrollPlugin(int id_to_make_visible) {
  // Plugin content doesn't scroll itself, so when we're requested to
  // scroll to make a particular plugin node visible, get the
  // coordinates of the target plugin node and then tell the document
  // node to scroll to those coordinates.
  //
  // Note that calling scrollToMakeVisibleWithSubFocus() is preferable to
  // telling the document to scroll to a specific coordinate because it will
  // first compute whether that rectangle is visible and do nothing if it is.
  // If it's not visible, it will automatically center it.

  DCHECK(plugin_tree_source_);
  ui::AXNodeData root_data = plugin_tree_source_->GetRoot()->data();
  ui::AXNodeData target_data =
      plugin_tree_source_->GetFromId(id_to_make_visible)->data();

  gfx::RectF bounds = target_data.location;
  if (root_data.transform)
    root_data.transform->TransformRect(&bounds);

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  WebAXObject::FromWebDocument(document).ScrollToMakeVisibleWithSubFocus(
      WebRect(bounds.x(), bounds.y(), bounds.width(), bounds.height()));
}

}  // namespace content
