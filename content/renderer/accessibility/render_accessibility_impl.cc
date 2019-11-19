// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/accessibility_messages.h"
#include "content/renderer/accessibility/ax_action_target_factory.h"
#include "content/renderer/accessibility/ax_image_annotator.h"
#include "content/renderer/accessibility/blink_ax_action_target.h"
#include "content/renderer/accessibility/blink_ax_enum_conversion.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_view_impl.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/accessibility_switches.h"
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

void SetAccessibilityCrashKey(ui::AXMode mode) {
  // Add a crash key with the ax_mode, to enable searching for top crashes that
  // occur when accessibility is turned on. This adds it for each renderer,
  // process, and elsewhere the same key is added for the browser process.
  // Note: in theory multiple renderers in the same process might not have the
  // same mode. As an example, kLabelImages could be enabled for just one
  // renderer. The presence if a mode flag means in a crash report means at
  // least one renderer in the same process had that flag.
  // Examples of when multiple renderers could share the same process:
  // 1) Android, 2) When many tabs are open.
  static auto* ax_mode_crash_key = base::debug::AllocateCrashKeyString(
      "ax_mode", base::debug::CrashKeySize::Size64);
  if (ax_mode_crash_key)
    base::debug::SetCrashKeyString(ax_mode_crash_key, mode.ToString());
}

// Returns the first language in the accept languages list.
std::string GetPreferredLanguage(const std::string& accept_languages) {
  const std::vector<std::string> tokens = base::SplitString(
      accept_languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return tokens.empty() ? "" : tokens[0];
}
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
      reset_token_(0) {
  ack_token_ = g_next_ack_token++;
  WebView* web_view = render_frame_->GetRenderView()->GetWebView();
  WebSettings* settings = web_view->GetSettings();

  SetAccessibilityCrashKey(mode);
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
    ax_context_ = std::make_unique<WebAXContext>(document);
    StartOrStopLabelingImages(ui::AXMode(), mode);

    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by sending it a notification.
    HandleAXEvent(WebAXObject::FromWebDocument(document),
                  ax::mojom::Event::kLayoutComplete);
  }

  image_annotation_debugging_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExperimentalAccessibilityLabelsDebugging);

  if (render_frame->render_view()) {
    render_frame_->render_view()->RegisterRendererPreferenceWatcher(
        pref_watcher_receiver_.BindNewPipeAndPassRemote());
  }
}

RenderAccessibilityImpl::~RenderAccessibilityImpl() = default;

void RenderAccessibilityImpl::DidCreateNewDocument() {
  const WebDocument& document = GetMainDocument();
  if (!document.IsNull())
    ax_context_ = std::make_unique<WebAXContext>(document);
}

void RenderAccessibilityImpl::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  has_injected_stylesheet_ = false;
  // Remove the image annotator if the page is loading and it was added for
  // the one-shot image annotation (i.e. AXMode for image annotation is not
  // set).
  if (!ax_image_annotator_ ||
      tree_source_.accessibility_mode().has_mode(ui::AXMode::kLabelImages)) {
    return;
  }
  tree_source_.RemoveImageAnnotator();
  ax_image_annotator_->Destroy();
  ax_image_annotator_.release();
}

void RenderAccessibilityImpl::AccessibilityModeChanged(const ui::AXMode& mode) {
  ui::AXMode old_mode = tree_source_.accessibility_mode();
  if (old_mode == mode)
    return;
  tree_source_.SetAccessibilityMode(mode);

  SetAccessibilityCrashKey(mode);

#if !defined(OS_ANDROID)
  // Inline text boxes can be enabled globally on all except Android.
  // On Android they can be requested for just a specific node.
  RenderView* render_view = render_frame_->GetRenderView();
  if (render_view) {
    WebView* web_view = render_view->GetWebView();
    if (web_view) {
      WebSettings* settings = web_view->GetSettings();
      if (settings) {
        if (mode.has_mode(ui::AXMode::kInlineTextBoxes)) {
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
    StartOrStopLabelingImages(old_mode, mode);

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
  IPC_BEGIN_MESSAGE_MAP(RenderAccessibilityImpl, message)

    IPC_MESSAGE_HANDLER(AccessibilityMsg_PerformAction, OnPerformAction)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_EventBundle_ACK, OnEventsAck)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_FatalError, OnFatalError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderAccessibilityImpl::NotifyUpdate(
    blink::mojom::RendererPreferencesPtr new_prefs) {
  if (ax_image_annotator_)
    ax_image_annotator_->set_preferred_language(
        GetPreferredLanguage(new_prefs->accept_languages));
}

void RenderAccessibilityImpl::HandleWebAccessibilityEvent(
    const WebAXObject& obj,
    ax::mojom::Event event,
    ax::mojom::EventFrom event_from) {
  HandleAXEvent(obj, event, event_from);
}

void RenderAccessibilityImpl::MarkWebAXObjectDirty(const WebAXObject& obj,
                                                   bool subtree) {
  DirtyObject dirty_object;
  dirty_object.obj = obj;
  dirty_object.event_from = ax::mojom::EventFrom::kAction;
  dirty_objects_.push_back(dirty_object);

  if (subtree)
    serializer_.InvalidateSubtree(obj);

  ScheduleSendAccessibilityEventsIfNeeded();
}

void RenderAccessibilityImpl::HandleAccessibilityFindInPageResult(
    int identifier,
    int match_index,
    const WebAXObject& start_object,
    int start_offset,
    const WebAXObject& end_object,
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

void RenderAccessibilityImpl::HandleAccessibilityFindInPageTermination() {
  Send(new AccessibilityHostMsg_FindInPageTermination(routing_id()));
}

void RenderAccessibilityImpl::AccessibilityFocusedElementChanged(
    const WebElement& element) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  if (element.IsNull()) {
    // When focus is cleared, implicitly focus the document.
    // TODO(dmazzoni): Make Blink send this notification instead.
    HandleAXEvent(WebAXObject::FromWebDocument(document),
                  ax::mojom::Event::kBlur);
  }
}

void RenderAccessibilityImpl::HandleAXEvent(const WebAXObject& obj,
                                            ax::mojom::Event event,
                                            ax::mojom::EventFrom event_from,
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
        HandleAXEvent(webax_object, ax::mojom::Event::kLayoutComplete,
                      event_from);
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
  acc_event.event_from = event_from;
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
  if (!render_frame_ || !render_frame_->in_frame_tree())
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

int RenderAccessibilityImpl::GenerateAXID() {
  WebAXObject root = tree_source_.GetRoot();
  return root.GenerateAXID();
}

void RenderAccessibilityImpl::SetPluginTreeSource(
    PluginAXTreeSource* plugin_tree_source) {
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
    std::vector<WebAXObject> children;
    tree_source_.GetChildren(obj, &children);
    for (size_t i = 0; i < children.size(); ++i)
      objs_to_explore.push(children[i]);
  }
}

WebDocument RenderAccessibilityImpl::GetMainDocument() {
  if (render_frame_ && render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->GetDocument();
  return WebDocument();
}

void RenderAccessibilityImpl::SendPendingAccessibilityEvents() {
  TRACE_EVENT0("accessibility",
               "RenderAccessibilityImpl::SendPendingAccessibilityEvents");

  WebDocument document = GetMainDocument();
  if (document.IsNull())
    return;

  if (pending_events_.empty() && dirty_objects_.empty())
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

  // If there's a load complete message, we need to send image metrics.
  bool had_load_complete_messages = false;

  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);

  // Loop over each event and generate an updated event message.
  for (size_t i = 0; i < src_events.size(); ++i) {
    ui::AXEvent& event = src_events[i];
    if (event.event_type == ax::mojom::Event::kLayoutComplete)
      had_layout_complete_messages = true;

    if (event.event_type == ax::mojom::Event::kLoadComplete)
      had_load_complete_messages = true;

    auto obj = WebAXObject::FromWebDocumentByID(document, event.id);

    // Make sure the object still exists.
    if (!obj.UpdateLayoutAndCheckValidity())
      continue;

    // If it's ignored, find the first ancestor that's not ignored.
    while (!obj.IsDetached() && obj.AccessibilityIsIgnored()) {
      // There are 3 states of nodes that we care about here.
      // (x) Unignored, included in tree
      // [x] Ignored, included in tree
      // <x> Ignored, excluded from tree
      //
      // Consider the following tree :
      // ++(0) Role::kRootWebArea
      // ++++<1> Role::kIgnored
      // ++++++[2] Role::kGenericContainer <body>
      // ++++++++[3] Role::kGenericContainer with 'visibility: hidden'
      //
      // If we modify [3] to be 'visibility: visible', we will receive
      // Event::kChildrenChanged here for the Ignored parent [2].
      // We must re-serialize the Unignored parent node (0) due to this
      // change, but we must also re-serialize [2] since its children
      // have changed. <1> was never part of the ax tree, and therefore
      // does not need to be serialized.
      //
      // So on the way to the Unignored parent, ancestors that are
      // included in the tree must also be serialized.
      // Note that [3] will be serialized to (3) during :
      // |AXTreeSerializer<>::SerializeChangedNodes| when node [2] is
      // being serialized, since it will detect the Ignored state had
      // changed.
      //
      // Similarly, during Event::kTextChanged, if any Ignored,
      // but included in tree ancestor uses NameFrom::kContents,
      // they must also be re-serialized in case the name changed.
      if (obj.AccessibilityIsIncludedInTree()) {
        DirtyObject dirty_object;
        dirty_object.obj = obj;
        dirty_object.event_from = event.event_from;
        dirty_objects.push_back(dirty_object);
      }
      obj = obj.ParentObject();
    }

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

  // Popups have a document lifecycle managed separately from the main document
  // but we need to return a combined accessibility tree for both.
  // We ensured layout validity for the main document in the loop above; if a
  // popup is open, do the same for it.
  WebDocument popup_document = GetPopupDocument();
  if (!popup_document.IsNull()) {
    WebAXObject popup_root_obj = WebAXObject::FromWebDocument(popup_document);
    if (!popup_root_obj.UpdateLayoutAndCheckValidity()) {
      // If a popup is open but we can't ensure its validity, return without
      // sending an update bundle, the same as we would for a node in the main
      // document.
      return;
    }
  }

  // Keep track of if the host node for a plugin has been invalidated,
  // because if so, the plugin subtree will need to be re-serialized.
  bool invalidate_plugin_subtree = false;
  if (plugin_tree_source_ && !plugin_host_node_.IsDetached()) {
    invalidate_plugin_subtree = !serializer_.IsInClientTree(plugin_host_node_);
  }

  // Now serialize all dirty objects. Keep track of IDs serialized
  // so we don't have to serialize the same node twice.
  std::set<int32_t> already_serialized_ids;
  for (size_t i = 0; i < dirty_objects.size(); i++) {
    auto obj = dirty_objects[i].obj;
    // Dirty objects can be added using MarkWebAXObjectDirty(obj) from other
    // parts of the code as well, so we need to ensure the object still exists.
    if (!obj.UpdateLayoutAndCheckValidity())
      continue;
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

    if (update.node_id_to_clear > 0)
      invalidate_plugin_subtree = true;

    if (plugin_tree_source_)
      AddPluginTreeToUpdate(&update, invalidate_plugin_subtree);

    // For each node in the update, set the location in our map from
    // ids to locations.
    for (size_t j = 0; j < update.nodes.size(); ++j) {
      ui::AXNodeData& src = update.nodes[j];
      ui::AXRelativeBounds& dst = locations_[update.nodes[j].id];
      dst = src.relative_bounds;
    }

    for (size_t j = 0; j < update.nodes.size(); ++j)
      already_serialized_ids.insert(update.nodes[j].id);

    bundle.updates.push_back(update);

    if (had_load_complete_messages)
      RecordImageMetrics(&update);

    VLOG(1) << "Accessibility tree update:\n" << update.ToString();
  }

  Send(new AccessibilityHostMsg_EventBundle(routing_id(), bundle, reset_token_,
                                            ack_token_));
  reset_token_ = 0;

  if (had_layout_complete_messages)
    SendLocationChanges();

  if (had_load_complete_messages)
    has_injected_stylesheet_ = false;

  if (image_annotation_debugging_)
    AddImageAnnotationDebuggingAttributes(bundle.updates);
}

void RenderAccessibilityImpl::SendLocationChanges() {
  TRACE_EVENT0("accessibility", "RenderAccessibilityImpl::SendLocationChanges");

  std::vector<AccessibilityHostMsg_LocationChangeParams> messages;

  // Update layout on the root of the tree.
  WebAXObject root = tree_source_.GetRoot();
  if (!root.UpdateLayoutAndCheckValidity())
    return;

  // Do a breadth-first explore of the whole blink AX tree.
  std::unordered_map<int, ui::AXRelativeBounds> new_locations;
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
    std::vector<WebAXObject> children;
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

  std::unique_ptr<ui::AXActionTarget> target =
      AXActionTargetFactory::CreateFromNodeId(document, plugin_tree_source_,
                                              data.target_node_id);
  std::unique_ptr<ui::AXActionTarget> anchor =
      AXActionTargetFactory::CreateFromNodeId(document, plugin_tree_source_,
                                              data.anchor_node_id);
  std::unique_ptr<ui::AXActionTarget> focus =
      AXActionTargetFactory::CreateFromNodeId(document, plugin_tree_source_,
                                              data.focus_node_id);

  switch (data.action) {
    case ax::mojom::Action::kBlur:
      root.Focus();
      break;
    case ax::mojom::Action::kClearAccessibilityFocus:
      target->ClearAccessibilityFocus();
      break;
    case ax::mojom::Action::kDecrement:
      target->Decrement();
      break;
    case ax::mojom::Action::kDoDefault:
      target->Click();
      break;
    case ax::mojom::Action::kGetImageData:
      OnGetImageData(target.get(), data.target_rect.size());
      break;
    case ax::mojom::Action::kHitTest:
      DCHECK(data.hit_test_event_to_fire != ax::mojom::Event::kNone);
      OnHitTest(data.target_point, data.hit_test_event_to_fire,
                data.request_id);
      break;
    case ax::mojom::Action::kIncrement:
      target->Increment();
      break;
    case ax::mojom::Action::kScrollToMakeVisible:
      target->ScrollToMakeVisibleWithSubFocus(data.target_rect,
                                              data.horizontal_scroll_alignment,
                                              data.vertical_scroll_alignment);
      break;
    case ax::mojom::Action::kScrollToPoint:
      target->ScrollToGlobalPoint(data.target_point);
      break;
    case ax::mojom::Action::kLoadInlineTextBoxes:
      OnLoadInlineTextBoxes(target.get());
      break;
    case ax::mojom::Action::kFocus:
      target->Focus();
      break;
    case ax::mojom::Action::kSetAccessibilityFocus:
      target->SetAccessibilityFocus();
      break;
    case ax::mojom::Action::kSetScrollOffset:
      target->SetScrollOffset(
          WebPoint(data.target_point.x(), data.target_point.y()));
      break;
    case ax::mojom::Action::kSetSelection:
      anchor->SetSelection(anchor.get(), data.anchor_offset, focus.get(),
                           data.focus_offset);
      HandleAXEvent(root, ax::mojom::Event::kLayoutComplete);
      break;
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
      target->SetSequentialFocusNavigationStartingPoint();
      break;
    case ax::mojom::Action::kSetValue:
      target->SetValue(data.value);
      break;
    case ax::mojom::Action::kShowContextMenu:
      target->ShowContextMenu();
      break;
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollUp:
    case ax::mojom::Action::kScrollDown:
    case ax::mojom::Action::kScrollLeft:
    case ax::mojom::Action::kScrollRight:
      Scroll(target.get(), data.action);
      break;
    case ax::mojom::Action::kCustomAction:
    case ax::mojom::Action::kReplaceSelectedText:
    case ax::mojom::Action::kNone:
      NOTREACHED();
      break;
    case ax::mojom::Action::kGetTextLocation:
      break;
    case ax::mojom::Action::kAnnotatePageImages:
      // Ensure we aren't already labeling images, in which case this should
      // not change.
      if (!ax_image_annotator_) {
        CreateAXImageAnnotator();
        // Walk the tree to discover images, and mark them dirty so that
        // they get added to the annotator.
        MarkAllAXObjectsDirty(ax::mojom::Role::kImage);
      }
      break;
    case ax::mojom::Action::kSignalEndOfTest:
      // Wait for 100ms to allow pending events to come in
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));

      HandleAXEvent(root, ax::mojom::Event::kEndOfTest);
      break;
    case ax::mojom::Action::kShowTooltip:
    case ax::mojom::Action::kHideTooltip:
    case ax::mojom::Action::kInternalInvalidateTree:
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
    gfx::Point transformed_point = point;
    bool is_remote_frame = RenderFrameProxy::FromRoutingID(
        data.GetContentIntAttribute(AX_CONTENT_ATTR_CHILD_ROUTING_ID));
    if (is_remote_frame) {
      // Remote frames don't have access to the information from the visual
      // viewport regarding the visual viewport offset, so we adjust the
      // coordinates before sending them to the remote renderer.
      WebRect rect = obj.GetBoundsInFrameCoordinates();
      // The following transformation of the input point is naive, but works
      // fairly well. It will fail with CSS transforms that rotate or shear.
      // https://crbug.com/981959.
      WebView* web_view = render_frame_->GetRenderView()->GetWebView();
      blink::WebFloatPoint viewport_offset = web_view->VisualViewportOffset();
      transformed_point += gfx::Vector2d(viewport_offset.x, viewport_offset.y) -
                           gfx::Rect(rect).OffsetFromOrigin();
    }
    Send(new AccessibilityHostMsg_ChildFrameHitTestResult(
        routing_id(), action_request_id, transformed_point,
        data.GetContentIntAttribute(AX_CONTENT_ATTR_CHILD_ROUTING_ID),
        data.GetContentIntAttribute(
            AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID),
        event_to_fire));
    return;
  }

  // Otherwise, send an event on the node that was hit.
  HandleAXEvent(obj, event_to_fire, ax::mojom::EventFrom::kAction,
                action_request_id);
}

void RenderAccessibilityImpl::OnLoadInlineTextBoxes(
    const ui::AXActionTarget* target) {
  const BlinkAXActionTarget* blink_target =
      BlinkAXActionTarget::FromAXActionTarget(target);
  if (!blink_target)
    return;
  const WebAXObject& obj = blink_target->WebAXObject();

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

void RenderAccessibilityImpl::OnGetImageData(const ui::AXActionTarget* target,
                                             const gfx::Size& max_size) {
  const BlinkAXActionTarget* blink_target =
      BlinkAXActionTarget::FromAXActionTarget(target);
  if (!blink_target)
    return;
  const WebAXObject& obj = blink_target->WebAXObject();

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
  render_frame_ = nullptr;
  delete this;
}

void RenderAccessibilityImpl::AddPluginTreeToUpdate(
    AXContentTreeUpdate* update,
    bool invalidate_plugin_subtree) {
  const WebDocument& document = GetMainDocument();
  if (invalidate_plugin_subtree)
    plugin_serializer_->Reset();

  for (size_t i = 0; i < update->nodes.size(); ++i) {
    if (update->nodes[i].role == ax::mojom::Role::kEmbeddedObject) {
      plugin_host_node_ =
          WebAXObject::FromWebDocumentByID(document, update->nodes[i].id);

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

void RenderAccessibilityImpl::CreateAXImageAnnotator() {
  if (!render_frame_)
    return;
  mojo::PendingRemote<image_annotation::mojom::Annotator> annotator;
  render_frame()->GetBrowserInterfaceBroker()->GetInterface(
      annotator.InitWithNewPipeAndPassReceiver());

  const std::string preferred_language =
      render_frame()->render_view()
          ? GetPreferredLanguage(
                render_frame()->render_view()->GetAcceptLanguages())
          : std::string();
  ax_image_annotator_ = std::make_unique<AXImageAnnotator>(
      this, preferred_language, std::move(annotator));
  tree_source_.AddImageAnnotator(ax_image_annotator_.get());
}

void RenderAccessibilityImpl::StartOrStopLabelingImages(ui::AXMode old_mode,
                                                        ui::AXMode new_mode) {
  if (!render_frame_)
    return;

  if (!old_mode.has_mode(ui::AXMode::kLabelImages) &&
      new_mode.has_mode(ui::AXMode::kLabelImages)) {
    CreateAXImageAnnotator();
  } else if (old_mode.has_mode(ui::AXMode::kLabelImages) &&
             !new_mode.has_mode(ui::AXMode::kLabelImages)) {
    tree_source_.RemoveImageAnnotator();
    ax_image_annotator_->Destroy();
    ax_image_annotator_.release();
  }
}

void RenderAccessibilityImpl::MarkAllAXObjectsDirty(ax::mojom::Role role) {
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  base::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(tree_source_.GetRoot());
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    if (obj.Role() == role)
      MarkWebAXObjectDirty(obj, /* subtree */ false);

    std::vector<blink::WebAXObject> children;
    tree_source_.GetChildren(obj, &children);
    for (size_t i = 0; i < children.size(); ++i)
      objs_to_explore.push(children[i]);
  }
}

void RenderAccessibilityImpl::Scroll(const ui::AXActionTarget* target,
                                     ax::mojom::Action scroll_action) {
  gfx::Rect bounds = target->GetRelativeBounds();
  if (bounds.IsEmpty())
    return;

  gfx::Point initial = target->GetScrollOffset();
  gfx::Point min = target->MinimumScrollOffset();
  gfx::Point max = target->MaximumScrollOffset();

  // TODO(anastasi): This 4/5ths came from the Android implementation, revisit
  // to find the appropriate modifier to keep enough context onscreen after
  // scrolling.
  int page_x = std::max((int)(bounds.width() * 4 / 5), 1);
  int page_y = std::max((int)(bounds.height() * 4 / 5), 1);

  // Forward/backward defaults to down/up unless it can only be scrolled
  // horizontally.
  if (scroll_action == ax::mojom::Action::kScrollForward)
    scroll_action = max.y() > min.y() ? ax::mojom::Action::kScrollDown
                                      : ax::mojom::Action::kScrollRight;
  if (scroll_action == ax::mojom::Action::kScrollBackward)
    scroll_action = max.y() > min.y() ? ax::mojom::Action::kScrollUp
                                      : ax::mojom::Action::kScrollLeft;

  int x = initial.x();
  int y = initial.y();
  switch (scroll_action) {
    case ax::mojom::Action::kScrollUp:
      if (initial.y() == min.y())
        return;
      y = std::max(initial.y() - page_y, min.y());
      break;
    case ax::mojom::Action::kScrollDown:
      if (initial.y() == max.y())
        return;
      y = std::min(initial.y() + page_y, max.y());
      break;
    case ax::mojom::Action::kScrollLeft:
      if (initial.x() == min.x())
        return;
      x = std::max(initial.x() - page_x, min.x());
      break;
    case ax::mojom::Action::kScrollRight:
      if (initial.x() == max.x())
        return;
      x = std::min(initial.x() + page_x, max.x());
      break;
    default:
      NOTREACHED();
  }

  target->SetScrollOffset(gfx::Point(x, y));
}

void RenderAccessibilityImpl::RecordImageMetrics(AXContentTreeUpdate* update) {
  if (!render_frame_->accessibility_mode().has_mode(ui::AXMode::kScreenReader))
    return;
  float scale_factor = render_frame_->GetDeviceScaleFactor();
  for (size_t i = 0; i < update->nodes.size(); ++i) {
    ui::AXNodeData& node_data = update->nodes[i];
    if (node_data.role != ax::mojom::Role::kImage)
      continue;
    // Convert to DIPs based on screen scale factor.
    int width = node_data.relative_bounds.bounds.width() / scale_factor;
    int height = node_data.relative_bounds.bounds.height() / scale_factor;
    if (width == 0 || height == 0)
      continue;
    // We log the min size in a histogram with a max of 10000, so set a ceiling
    // of 10000 on min_size.
    int min_size = std::min({width, height, 10000});
    int max_size = std::max(width, height);
    // The ratio is always the smaller divided by the larger so as not to go
    // over 100%.
    int ratio = min_size * 100.0 / max_size;
    const std::string name =
        node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    bool explicitly_empty = node_data.GetNameFrom() ==
                            ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
    if (!name.empty()) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Accessibility.ScreenReader.Image.SizeRatio.Labeled", ratio);
      UMA_HISTOGRAM_COUNTS_10000(
          "Accessibility.ScreenReader.Image.MinSize.Labeled", min_size);
    } else if (explicitly_empty) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Accessibility.ScreenReader.Image.SizeRatio.ExplicitlyUnlabeled",
          ratio);
      UMA_HISTOGRAM_COUNTS_10000(
          "Accessibility.ScreenReader.Image.MinSize.ExplicitlyUnlabeled",
          min_size);
    } else {
      UMA_HISTOGRAM_PERCENTAGE(
          "Accessibility.ScreenReader.Image.SizeRatio.Unlabeled", ratio);
      UMA_HISTOGRAM_COUNTS_10000(
          "Accessibility.ScreenReader.Image.MinSize.Unlabeled", min_size);
    }
  }
}

void RenderAccessibilityImpl::AddImageAnnotationDebuggingAttributes(
    const std::vector<AXContentTreeUpdate>& updates) {
  DCHECK(image_annotation_debugging_);

  for (auto& update : updates) {
    for (auto& node : update.nodes) {
      if (!node.HasIntAttribute(
              ax::mojom::IntAttribute::kImageAnnotationStatus))
        continue;

      ax::mojom::ImageAnnotationStatus status = node.GetImageAnnotationStatus();
      bool should_set_attributes = false;
      switch (status) {
        case ax::mojom::ImageAnnotationStatus::kNone:
        case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
        case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
        case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
        case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
          break;
        case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
        case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
        case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
        case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
        case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
          should_set_attributes = true;
          break;
      }

      if (!should_set_attributes)
        continue;

      WebDocument document = GetMainDocument();
      if (document.IsNull())
        continue;
      WebAXObject obj = WebAXObject::FromWebDocumentByID(document, node.id);
      if (obj.IsDetached())
        continue;

      if (!has_injected_stylesheet_) {
        document.InsertStyleSheet(
            "[imageannotation=annotationPending] { outline: 3px solid #9ff; } "
            "[imageannotation=annotationSucceeded] { outline: 3px solid #3c3; "
            "} "
            "[imageannotation=annotationEmpty] { outline: 3px solid #ee6; } "
            "[imageannotation=annotationAdult] { outline: 3px solid #f90; } "
            "[imageannotation=annotationProcessFailed] { outline: 3px solid "
            "#c00; } ");
        has_injected_stylesheet_ = true;
      }

      WebNode web_node = obj.GetNode();
      if (web_node.IsNull() || !web_node.IsElementNode())
        continue;

      WebElement element = web_node.To<WebElement>();
      std::string status_str = ui::ToString(status);
      if (element.GetAttribute("imageannotation").Utf8() != status_str)
        element.SetAttribute("imageannotation",
                             blink::WebString::FromUTF8(status_str));

      std::string title = "%" + status_str;
      std::string annotation =
          node.GetStringAttribute(ax::mojom::StringAttribute::kImageAnnotation);
      if (!annotation.empty())
        title = title + ": " + annotation;
      if (element.GetAttribute("title").Utf8() != title) {
        element.SetAttribute("title", blink::WebString::FromUTF8(title));
      }
    }
  }
}

blink::WebDocument RenderAccessibilityImpl::GetPopupDocument() {
  blink::WebPagePopup* popup =
      render_frame_->GetRenderView()->GetWebView()->GetPagePopup();
  if (popup)
    return popup->GetDocument();
  return WebDocument();
}

}  // namespace content
