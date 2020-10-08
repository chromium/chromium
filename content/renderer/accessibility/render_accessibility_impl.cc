// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/accessibility/ax_action_target_factory.h"
#include "content/renderer/accessibility/ax_image_annotator.h"
#include "content/renderer/accessibility/blink_ax_action_target.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_view_impl.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_disallow_transition_scope.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_event_intent.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

using blink::WebAXContext;
using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebRect;
using blink::WebSettings;
using blink::WebView;

namespace {

// The amount of time, in milliseconds, to wait before sending accessibility
// events that are deferred rather than being sent right away. As one
// example this is used during initial page load.
constexpr int kDelayForDeferredEvents = 350;

// The minimum amount of time in milliseconds that should be spent
// in serializing code in order to report the elapsed time as a URL-keyed
// metric.
constexpr int kMinSerializationTimeToSendInMS = 100;

// When URL-keyed metrics for the amount of time spent in serializing code
// are sent, the minimum amount of time to wait, in seconds, before
// sending metrics. Metrics may also be sent once per page transition.
constexpr int kMinUKMDelayInSeconds = 300;

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

}

namespace content {

// Cap the number of nodes returned in an accessibility
// tree snapshot to avoid outrageous memory or bandwidth
// usage.
const size_t kMaxSnapshotNodeCount = 5000;

AXTreeSnapshotterImpl::AXTreeSnapshotterImpl(RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {
  DCHECK(render_frame->GetWebFrame());
  blink::WebDocument document_ = render_frame->GetWebFrame()->GetDocument();
  context_ = std::make_unique<WebAXContext>(document_);
}

AXTreeSnapshotterImpl::~AXTreeSnapshotterImpl() = default;

void AXTreeSnapshotterImpl::Snapshot(ui::AXMode ax_mode,
                                     size_t max_node_count,
                                     ui::AXTreeUpdate* response) {
  // Get a snapshot of the accessibility tree as an AXNodeData.
  ui::AXTreeUpdate content_tree;
  SnapshotContentTree(ax_mode, max_node_count, &content_tree);

  // As a sanity check, node_id_to_clear and event_from should be uninitialized
  // if this is a full tree snapshot. They'd only be set to something if
  // this was indeed a partial update to the tree (which we don't want).
  DCHECK_EQ(0, content_tree.node_id_to_clear);
  DCHECK_EQ(ax::mojom::EventFrom::kNone, content_tree.event_from);

  // We now have a complete serialization of the accessibility tree, but it
  // includes a few fields we don't want to export outside of content/,
  // so copy it into a more generic ui::AXTreeUpdate instead.
  response->root_id = content_tree.root_id;
  response->nodes.resize(content_tree.nodes.size());
  response->node_id_to_clear = content_tree.node_id_to_clear;
  response->event_from = content_tree.event_from;
  response->nodes.assign(content_tree.nodes.begin(), content_tree.nodes.end());
}

void AXTreeSnapshotterImpl::SnapshotContentTree(ui::AXMode ax_mode,
                                                size_t max_node_count,
                                                ui::AXTreeUpdate* response) {
  if (!render_frame_->GetWebFrame())
    return;
  if (!WebAXObject::MaybeUpdateLayoutAndCheckValidity(
          render_frame_->GetWebFrame()->GetDocument()))
    return;
  WebAXObject root = context_->Root();

  BlinkAXTreeSource tree_source(render_frame_, ax_mode);
  tree_source.SetRoot(root);
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source);

  // The serializer returns an ui::AXTreeUpdate, which can store a complete
  // or a partial accessibility tree. AXTreeSerializer is stateful, but the
  // first time you serialize from a brand-new tree you're guaranteed to get a
  // complete tree.
  BlinkAXTreeSerializer serializer(&tree_source);
  if (max_node_count)
    serializer.set_max_node_count(max_node_count);
  if (serializer.SerializeChanges(root, response))
    return;

  // It's possible for the page to fail to serialize the first time due to
  // aria-owns rearranging the page while it's being scanned. Try a second
  // time.
  *response = ui::AXTreeUpdate();
  if (serializer.SerializeChanges(root, response))
    return;

  // It failed again. Clear the response object because it might have errors.
  *response = ui::AXTreeUpdate();
  LOG(WARNING) << "Unable to serialize accessibility tree.";
}

// static
void RenderAccessibilityImpl::SnapshotAccessibilityTree(
    RenderFrameImpl* render_frame,
    ui::AXTreeUpdate* response,
    ui::AXMode ax_mode) {
  TRACE_EVENT0("accessibility",
               "RenderAccessibilityImpl::SnapshotAccessibilityTree");
  DCHECK(render_frame);
  DCHECK(response);
  if (!render_frame->GetWebFrame())
    return;

  AXTreeSnapshotterImpl snapshotter(render_frame);
  snapshotter.SnapshotContentTree(ax_mode, kMaxSnapshotNodeCount, response);
}

RenderAccessibilityImpl::RenderAccessibilityImpl(
    RenderAccessibilityManager* const render_accessibility_manager,
    RenderFrameImpl* const render_frame,
    ui::AXMode mode)
    : RenderFrameObserver(render_frame),
      render_accessibility_manager_(render_accessibility_manager),
      render_frame_(render_frame),
      tree_source_(std::make_unique<BlinkAXTreeSource>(render_frame, mode)),
      serializer_(std::make_unique<BlinkAXTreeSerializer>(tree_source_.get())),
      plugin_tree_source_(nullptr),
      last_scroll_offset_(gfx::Size()),
      event_schedule_status_(EventScheduleStatus::kNotWaiting),
      reset_token_(0),
      ukm_timer_(std::make_unique<base::ElapsedTimer>()),
      last_ukm_source_id_(ukm::kInvalidSourceId) {
  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
  content::RenderThread::Get()->BindHostReceiver(
      recorder.InitWithNewPipeAndPassReceiver());
  ukm_recorder_ = std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
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

#if defined(OS_MAC)
  // aria-modal currently prunes the accessibility tree on Mac only.
  settings->SetAriaModalPrunesAXTree(true);
#endif

#if defined(OS_CHROMEOS)
  // Do not ignore SVG grouping (<g>) elements on ChromeOS, which is needed so
  // Select-to-Speak can read SVG text nodes in natural reading order.
  settings->SetAccessibilityIncludeSvgGElement(true);
#endif

  if (render_frame_->IsMainFrame())
    event_schedule_mode_ = EventScheduleMode::kDeferEvents;
  else
    event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;

  // Optionally disable AXMenuList, which makes the internal pop-up menu
  // UI for a select element directly accessible. Disable by default on
  // Chrome OS, but some tests may override.
  bool disable_ax_menu_list = false;
#if defined(OS_CHROMEOS)
  disable_ax_menu_list = true;
#endif
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kDisableAXMenuList)) {
    if (command_line->GetSwitchValueASCII(::switches::kDisableAXMenuList) ==
        "false")
      disable_ax_menu_list = false;
    else
      disable_ax_menu_list = true;
  }
  if (disable_ax_menu_list)
    settings->SetUseAXMenuList(false);

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    ax_context_ = std::make_unique<WebAXContext>(document);
    StartOrStopLabelingImages(ui::AXMode(), mode);

    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by firing a layout complete for the document.
    // Ensure that this occurs after initial layout is actually complete.
    ScheduleSendPendingAccessibilityEvents();
  }

  image_annotation_debugging_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExperimentalAccessibilityLabelsDebugging);
}

RenderAccessibilityImpl::~RenderAccessibilityImpl() = default;

void RenderAccessibilityImpl::DidCreateNewDocument() {
  const WebDocument& document = GetMainDocument();
  if (!document.IsNull())
    ax_context_ = std::make_unique<WebAXContext>(document);
}

void RenderAccessibilityImpl::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  has_injected_stylesheet_ = false;

  // If we have events scheduled, but not sent, cancel them
  CancelScheduledEvents();
  // Defer events during initial page load.
  event_schedule_mode_ = EventScheduleMode::kDeferEvents;

  MaybeSendUKM();
  slowest_serialization_ms_ = 0;
  ukm_timer_ = std::make_unique<base::ElapsedTimer>();

  // Remove the image annotator if the page is loading and it was added for
  // the one-shot image annotation (i.e. AXMode for image annotation is not
  // set).
  if (!ax_image_annotator_ ||
      GetAccessibilityMode().has_mode(ui::AXMode::kLabelImages)) {
    return;
  }
  tree_source_->RemoveImageAnnotator();
  ax_image_annotator_->Destroy();
  ax_image_annotator_.release();
  page_language_.clear();
}

void RenderAccessibilityImpl::AccessibilityModeChanged(const ui::AXMode& mode) {
  ui::AXMode old_mode = GetAccessibilityMode();
  if (old_mode == mode)
    return;
  tree_source_->SetAccessibilityMode(mode);

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
          tree_source_->GetRoot().MaybeUpdateLayoutAndCheckValidity();
          tree_source_->GetRoot().LoadInlineTextBoxes();
        } else {
          settings->SetInlineTextBoxAccessibilityEnabled(false);
        }
      }
    }
  }
#endif  // !defined(OS_ANDROID)

  serializer_->Reset();
  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    StartOrStopLabelingImages(old_mode, mode);

    // If there are any events in flight, |HandleAXEvent| will refuse to process
    // our new event.
    pending_events_.clear();
    auto root_object = WebAXObject::FromWebDocument(document, false);
    ax::mojom::Event event = root_object.IsLoaded()
                                 ? ax::mojom::Event::kLoadComplete
                                 : ax::mojom::Event::kLayoutComplete;
    HandleAXEvent(ui::AXEvent(root_object.AxID(), event));
  }
}

void RenderAccessibilityImpl::HitTest(
    const gfx::Point& point,
    ax::mojom::Event event_to_fire,
    int request_id,
    mojom::RenderAccessibility::HitTestCallback callback) {
  WebAXObject ax_object;
  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    auto root_obj = WebAXObject::FromWebDocument(document);
    if (root_obj.MaybeUpdateLayoutAndCheckValidity())
      ax_object = root_obj.HitTest(point);
  }

  // Return if no attached accessibility object was found for the main document.
  if (ax_object.IsDetached()) {
    std::move(callback).Run(/*hit_test_response=*/nullptr);
    return;
  }

  // If the result was in the same frame, return the result.
  ui::AXNodeData data;
  ScopedFreezeBlinkAXTreeSource freeze(tree_source_.get());
  tree_source_->SerializeNode(ax_object, &data);
  if (!data.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    // Optionally fire an event, if requested to. This is a good fit for
    // features like touch exploration on Android, Chrome OS, and
    // possibly other platforms - if the user explore a particular point,
    // we fire a hover event on the nearest object under the point.
    //
    // Avoid using this mechanism to fire a particular sentinel event
    // and then listen for that event to associate it with the hit test
    // request. Instead, the mojo reply should be used directly.
    if (event_to_fire != ax::mojom::Event::kNone) {
      const std::vector<ui::AXEventIntent> intents;
      HandleAXEvent(ui::AXEvent(ax_object.AxID(), event_to_fire,
                                ax::mojom::EventFrom::kAction, intents,
                                request_id));
    }

    // Reply with the result.
    const auto& frame_token = render_frame_->GetWebFrame()->GetFrameToken();
    std::move(callback).Run(
        mojom::HitTestResponse::New(frame_token, point, ax_object.AxID()));
    return;
  }

  // The result was in a child frame. Reply so that the
  // client can do a hit test on the child frame recursively.
  // If it's a remote frame, transform the point into the child frame's
  // coordinate system.
  gfx::Point transformed_point = point;
  blink::WebFrame* child_frame =
      blink::WebFrame::FromFrameOwnerElement(ax_object.GetNode());
  DCHECK(child_frame);

  if (child_frame->IsWebRemoteFrame()) {
    // Remote frames don't have access to the information from the visual
    // viewport regarding the visual viewport offset, so we adjust the
    // coordinates before sending them to the remote renderer.
    WebRect rect = ax_object.GetBoundsInFrameCoordinates();
    // The following transformation of the input point is naive, but works
    // fairly well. It will fail with CSS transforms that rotate or shear.
    // https://crbug.com/981959.
    WebView* web_view = render_frame_->GetRenderView()->GetWebView();
    gfx::PointF viewport_offset = web_view->VisualViewportOffset();
    transformed_point +=
        gfx::Vector2d(viewport_offset.x(), viewport_offset.y()) -
        gfx::Rect(rect).OffsetFromOrigin();
  }

  std::move(callback).Run(mojom::HitTestResponse::New(
      child_frame->GetFrameToken(), transformed_point, ax_object.AxID()));
}

void RenderAccessibilityImpl::PerformAction(const ui::AXActionData& data) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  auto root = WebAXObject::FromWebDocument(document);
  if (!root.MaybeUpdateLayoutAndCheckValidity())
    return;

  // If an action was requested, we no longer want to defer events.
  event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;

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
    case ax::mojom::Action::kIncrement:
      target->Increment();
      break;
    case ax::mojom::Action::kScrollToMakeVisible:
      target->ScrollToMakeVisibleWithSubFocus(
          data.target_rect, data.horizontal_scroll_alignment,
          data.vertical_scroll_alignment, data.scroll_behavior);
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
      target->SetScrollOffset(data.target_point);
      break;
    case ax::mojom::Action::kSetSelection:
      anchor->SetSelection(anchor.get(), data.anchor_offset, focus.get(),
                           data.focus_offset);
      HandleAXEvent(
          ui::AXEvent(root.AxID(), ax::mojom::Event::kLayoutComplete));
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
    case ax::mojom::Action::kCollapse:
    case ax::mojom::Action::kExpand:
    case ax::mojom::Action::kHitTest:
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

      HandleAXEvent(ui::AXEvent(root.AxID(), ax::mojom::Event::kEndOfTest));
      break;
    case ax::mojom::Action::kShowTooltip:
    case ax::mojom::Action::kHideTooltip:
    case ax::mojom::Action::kInternalInvalidateTree:
      break;
  }
}

void RenderAccessibilityImpl::Reset(int32_t reset_token) {
  reset_token_ = reset_token;
  serializer_->Reset();
  pending_events_.clear();

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    // Tree-only mode gets used by the automation extension API which requires a
    // load complete event to invoke listener callbacks.
    auto root_object = WebAXObject::FromWebDocument(document, false);
    ax::mojom::Event event = root_object.IsLoaded()
                                 ? ax::mojom::Event::kLoadComplete
                                 : ax::mojom::Event::kLayoutComplete;
    HandleAXEvent(ui::AXEvent(root_object.AxID(), event));
  }
}

void RenderAccessibilityImpl::HandleWebAccessibilityEvent(
    const ui::AXEvent& event) {
  HandleAXEvent(event);
}

void RenderAccessibilityImpl::MarkWebAXObjectDirty(const WebAXObject& obj,
                                                   bool subtree) {
  DirtyObject dirty_object;
  dirty_object.obj = obj;
  dirty_object.event_from = ax::mojom::EventFrom::kAction;
  dirty_objects_.push_back(dirty_object);

  if (subtree)
    serializer_->InvalidateSubtree(obj);

  ScheduleSendPendingAccessibilityEvents();
}

void RenderAccessibilityImpl::HandleAXEvent(const ui::AXEvent& event) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  auto obj = WebAXObject::FromWebDocumentByID(document, event.id);
  if (obj.IsDetached())
    return;

#if defined(OS_ANDROID)
  // Force the newly focused node to be re-serialized so we include its
  // inline text boxes.
  if (event.event_type == ax::mojom::Event::kFocus)
    serializer_->InvalidateSubtree(obj);
#endif

  // If a select tag is opened or closed, all the children must be updated
  // because their visibility may have changed.
  if (obj.Role() == ax::mojom::Role::kMenuListPopup &&
      event.event_type == ax::mojom::Event::kChildrenChanged) {
    WebAXObject popup_like_object = obj.ParentObject();
    if (!popup_like_object.IsDetached()) {
      serializer_->InvalidateSubtree(popup_like_object);
      HandleAXEvent(ui::AXEvent(popup_like_object.AxID(),
                                ax::mojom::Event::kChildrenChanged));
    }
  }

  // Discard duplicate accessibility events.
  for (const ui::AXEvent& pending_event : pending_events_) {
    if (pending_event.id == event.id &&
        pending_event.event_type == event.event_type) {
      return;
    }
  }
  pending_events_.push_back(event);

  // Once we get the first load, we should no longer defer events.
  if (event.event_type == ax::mojom::Event::kLoadComplete)
    event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;

  ScheduleSendPendingAccessibilityEvents();
}

bool RenderAccessibilityImpl::ShouldSerializeNodeForEvent(
    const WebAXObject& obj,
    const ui::AXEvent& event) const {
  if (obj.IsDetached())
    return false;

  if (event.event_type == ax::mojom::Event::kTextSelectionChanged &&
      !obj.IsNativeTextControl()) {
    // Selection changes on non-native text controls cause no change to the
    // control node's data.
    //
    // Selection offsets exposed via kTextSelStart and kTextSelEnd are only used
    // for plain text controls, (input of a text field type, and textarea). Rich
    // editable areas, such as contenteditables, use AXTreeData.
    //
    // TODO(nektar): Remove kTextSelStart and kTextSelEnd from the renderer.
    return false;
  }

  return true;
}

void RenderAccessibilityImpl::ScheduleSendPendingAccessibilityEvents(
    bool scheduling_from_task) {
  // Don't send accessibility events for frames that are not in the frame tree
  // yet (i.e., provisional frames used for remote-to-local navigations, which
  // haven't committed yet).  Doing so might trigger layout, which may not work
  // correctly for those frames.  The events should be sent once such a frame
  // commits.
  if (!render_frame_ || !render_frame_->in_frame_tree())
    return;

  switch (event_schedule_status_) {
    case EventScheduleStatus::kScheduledDeferred:
      if (event_schedule_mode_ ==
          EventScheduleMode::kProcessEventsImmediately) {
        // Cancel scheduled deferred events so we can schedule events to be
        // sent immediately.
        CancelScheduledEvents();
        break;
      }
      // We have already scheduled a task to send pending events.
      return;
    case EventScheduleStatus::kScheduledImmediate:
      // The send pending events task have been scheduled, but has not started.
      return;
    case EventScheduleStatus::kWaitingForAck:
      // Events have been sent, wait for ack.
      return;
    case EventScheduleStatus::kNotWaiting:
      // Once the events have been handled, we schedule the pending events from
      // that task. In this case, there would be a weak ptr still in use.
      if (!scheduling_from_task &&
          weak_factory_for_pending_events_.HasWeakPtrs())
        return;
      break;
  }

  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(0);
  switch (event_schedule_mode_) {
    case EventScheduleMode::kDeferEvents:
      event_schedule_status_ = EventScheduleStatus::kScheduledDeferred;
      // During page load, process changes on a delay so that they occur in
      // larger batches, which helps improve efficiency of page loads.
      delay = base::TimeDelta::FromMilliseconds(kDelayForDeferredEvents);
      break;
    case EventScheduleMode::kProcessEventsImmediately:
      event_schedule_status_ = EventScheduleStatus::kScheduledImmediate;
      delay = base::TimeDelta::FromMilliseconds(0);
      break;
  }

  // When no accessibility events are in-flight post a task to send
  // the events to the browser. We use PostTask so that we can queue
  // up additional events.
  render_frame_->GetTaskRunner(blink::TaskType::kInternalDefault)
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &RenderAccessibilityImpl::SendPendingAccessibilityEvents,
              weak_factory_for_pending_events_.GetWeakPtr()),
          delay);
}

int RenderAccessibilityImpl::GenerateAXID() {
  WebAXObject root = tree_source_->GetRoot();
  return root.GenerateAXID();
}

void RenderAccessibilityImpl::SetPluginTreeSource(
    PluginAXTreeSource* plugin_tree_source) {
  plugin_tree_source_ = plugin_tree_source;
  plugin_serializer_.reset(new PluginAXTreeSerializer(plugin_tree_source_));

  OnPluginRootNodeUpdated();
}

void RenderAccessibilityImpl::OnPluginRootNodeUpdated() {
  // Search the accessibility tree for plugin's root object and post a
  // children changed notification on it to force it to update the
  // plugin accessibility tree.
  WebAXObject obj = GetPluginRoot();
  if (obj.IsNull())
    return;

  HandleAXEvent(ui::AXEvent(obj.AxID(), ax::mojom::Event::kChildrenChanged));
}

void RenderAccessibilityImpl::ShowPluginContextMenu() {
  // Search the accessibility tree for plugin's root object and invoke
  // ShowContextMenu() on it to show context menu for plugin.
  WebAXObject obj = GetPluginRoot();
  if (obj.IsNull())
    return;

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  std::unique_ptr<ui::AXActionTarget> target =
      AXActionTargetFactory::CreateFromNodeId(document, plugin_tree_source_,
                                              obj.AxID());
  target->ShowContextMenu();
}

WebDocument RenderAccessibilityImpl::GetMainDocument() {
  if (render_frame_ && render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->GetDocument();
  return WebDocument();
}

std::string RenderAccessibilityImpl::GetLanguage() {
  return page_language_;
}

void RenderAccessibilityImpl::SendPendingAccessibilityEvents() {
  TRACE_EVENT0("accessibility",
               "RenderAccessibilityImpl::SendPendingAccessibilityEvents");
  base::ElapsedTimer timer;

  // Clear status here in case we return early.
  event_schedule_status_ = EventScheduleStatus::kNotWaiting;
  WebDocument document = GetMainDocument();
  if (document.IsNull())
    return;

  if (needs_initial_ax_tree_root_) {
    // At the very start of accessibility for this document, push a layout
    // complete for the entire document, in order to initialize the browser's
    // cached accessibility tree.
    needs_initial_ax_tree_root_ = false;
    auto obj = WebAXObject::FromWebDocument(document);
    pending_events_.insert(
        pending_events_.begin(),
        ui::AXEvent(obj.AxID(), ax::mojom::Event::kLayoutComplete));
  }

  if (pending_events_.empty() && dirty_objects_.empty())
    return;

  // Update layout before snapshotting the events so that live state read from
  // the DOM during freezing (e.g. which node currently has focus) is consistent
  // with the events and node data we're about to send up.
  WebAXObject::UpdateLayout(document);

  // Make a copy of the events, because it's possible that
  // actions inside this loop will cause more events to be
  // queued up.
  std::vector<ui::AXEvent> src_events = pending_events_;
  pending_events_.clear();

  // The serialized list of updates and events to send to the browser.
  std::vector<ui::AXTreeUpdate> updates;
  std::vector<ui::AXEvent> events;

  // Keep track of nodes in the tree that need to be updated.
  std::vector<DirtyObject> dirty_objects = dirty_objects_;
  dirty_objects_.clear();

  // If there's a layout complete or a scroll changed message, we need to send
  // location changes.
  bool need_to_send_location_changes = false;

  // If there's a load complete message, we need to change the event schedule
  // mode.
  bool had_load_complete_messages = false;

  ScopedFreezeBlinkAXTreeSource freeze(tree_source_.get());

  WebAXObject root = tree_source_->GetRoot();
#if DCHECK_IS_ON()
  // Never causes a document lifecycle change during serialization,
  // because the assumption is that layout is in a safe, stable state.
  blink::WebDisallowTransitionScope disallow(&document);
#endif

  // Save the page language.
  page_language_ = root.Language().Utf8();

  // Loop over each event and generate an updated event message.
  for (ui::AXEvent& event : src_events) {
    if (event.event_type == ax::mojom::Event::kLayoutComplete ||
        event.event_type == ax::mojom::Event::kScrollPositionChanged) {
      need_to_send_location_changes = true;
    }

    if (event.event_type == ax::mojom::Event::kLoadComplete)
      had_load_complete_messages = true;

    auto obj = WebAXObject::FromWebDocumentByID(document, event.id);

    // Make sure the object still exists.
    // TODO(accessibility) Change this to CheckValidity() if there aren't crash
    // reports of illegal lifecycle changes from WebDisallowTransitionScope.
    if (!obj.MaybeUpdateLayoutAndCheckValidity())
      continue;

    // Make sure it's a descendant of our root node - exceptions include the
    // scroll area that's the parent of the main document (we ignore it), and
    // possibly nodes attached to a different document.
    if (!tree_source_->IsInTree(obj))
      continue;

    // If it's ignored, find the first ancestor that's not ignored.
    //
    // Note that "IsDetached()" also calls "IsNull()". Additionally,
    // "ParentObject()" always gets the first ancestor that is included in tree
    // (ignored or unignored), so it will never return objects that are not
    // included in the tree at all.
    if (!obj.AccessibilityIsIncludedInTree())
      obj = obj.ParentObject();
    for (; !obj.IsDetached() && obj.AccessibilityIsIgnored();
         obj = obj.ParentObject()) {
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
      // Note that [3] will be serialized to (3) during :
      // |AXTreeSerializer<>::SerializeChangedNodes| when node [2] is
      // being serialized, since it will detect the Ignored state had
      // changed.
      //
      // Similarly, during Event::kTextChanged, if any Ignored,
      // but included in tree ancestor uses NameFrom::kContents,
      // they must also be re-serialized in case the name changed.
      if (ShouldSerializeNodeForEvent(obj, event)) {
        DirtyObject dirty_object;
        dirty_object.obj = obj;
        dirty_object.event_from = event.event_from;
        dirty_object.event_intents = event.event_intents;
        dirty_objects.push_back(dirty_object);
      }
    }

    events.push_back(event);

    VLOG(1) << "Accessibility event: " << ui::ToString(event.event_type)
            << " on node id " << event.id;

    // Some events don't cause any changes to their associated objects.
    if (ShouldSerializeNodeForEvent(obj, event)) {
      DirtyObject dirty_object;
      dirty_object.obj = obj;
      dirty_object.event_from = event.event_from;
      dirty_object.event_intents = event.event_intents;
      dirty_objects.push_back(dirty_object);
    }
  }

  // Popups have a document lifecycle managed separately from the main document
  // but we need to return a combined accessibility tree for both.
  // We ensured layout validity for the main document in the loop above; if a
  // popup is open, do the same for it.
  WebDocument popup_document = GetPopupDocument();
  if (!popup_document.IsNull()) {
    WebAXObject popup_root_obj = WebAXObject::FromWebDocument(popup_document);
    if (!popup_root_obj.MaybeUpdateLayoutAndCheckValidity()) {
      // If a popup is open but we can't ensure its validity, return without
      // sending an update bundle, the same as we would for a node in the main
      // document.
      return;
    }
  }

#if DCHECK_IS_ON()
  // Protect against lifecycle changes in the popup document, if any.
  // If no popup document, use the main document -- it's harmless to protect it
  // twice, and some document is needed because this cannot be done in an if
  // statement because it's scoped.
  WebDocument popup_or_main_document =
      popup_document.IsNull() ? document : popup_document;
  blink::WebDisallowTransitionScope disallow2(&popup_or_main_document);
#endif

  // Keep track of if the host node for a plugin has been invalidated,
  // because if so, the plugin subtree will need to be re-serialized.
  bool invalidate_plugin_subtree = false;
  if (plugin_tree_source_ && !plugin_host_node_.IsDetached()) {
    invalidate_plugin_subtree = !serializer_->IsInClientTree(plugin_host_node_);
  }

  // Now serialize all dirty objects. Keep track of IDs serialized
  // so we don't have to serialize the same node twice.
  std::set<int32_t> already_serialized_ids;
  for (size_t i = 0; i < dirty_objects.size(); i++) {
    auto obj = dirty_objects[i].obj;
    // Dirty objects can be added using MarkWebAXObjectDirty(obj) from other
    // parts of the code as well, so we need to ensure the object still exists.
    // TODO(accessibility) Change this to CheckValidity() if there aren't crash
    // reports of illegal lifecycle changes from WebDisallowTransitionScope.
    if (!obj.MaybeUpdateLayoutAndCheckValidity())
      continue;

    // If the object in question is not included in the tree, get the
    // nearest ancestor that is (ParentObject() will do this for us).
    // Otherwise this can lead to the serializer doing extra work because
    // the object won't be in |already_serialized_ids|.
    if (!obj.AccessibilityIsIncludedInTree()) {
      obj = obj.ParentObject();
      if (obj.IsDetached())
        continue;
    }

    if (already_serialized_ids.find(obj.AxID()) != already_serialized_ids.end())
      continue;

    ui::AXTreeUpdate update;
    update.event_from = dirty_objects[i].event_from;
    update.event_intents = dirty_objects[i].event_intents;
    // If there's a plugin, force the tree data to be generated in every
    // message so the plugin can merge its own tree data changes.
    if (plugin_tree_source_)
      update.has_tree_data = true;

    if (!serializer_->SerializeChanges(obj, &update)) {
      VLOG(1) << "Failed to serialize one accessibility event.";
      continue;
    }

    if (update.node_id_to_clear > 0)
      invalidate_plugin_subtree = true;

    if (plugin_tree_source_)
      AddPluginTreeToUpdate(&update, invalidate_plugin_subtree);

    for (auto& node : update.nodes)
      already_serialized_ids.insert(node.id);

    updates.push_back(update);

    VLOG(1) << "Accessibility tree update:\n" << update.ToString();
  }

  event_schedule_status_ = EventScheduleStatus::kWaitingForAck;
  render_accessibility_manager_->HandleAccessibilityEvents(
      updates, events, reset_token_,
      base::BindOnce(&RenderAccessibilityImpl::OnAccessibilityEventsHandled,
                     weak_factory_for_pending_events_.GetWeakPtr()));
  reset_token_ = 0;

  if (need_to_send_location_changes)
    SendLocationChanges();

  if (had_load_complete_messages) {
    has_injected_stylesheet_ = false;
    event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;
  }

  if (image_annotation_debugging_)
    AddImageAnnotationDebuggingAttributes(updates);

  // Measure the amount of time spent in this function. Keep track of the
  // maximum within a time interval so we can upload UKM.
  int elapsed_time_ms = timer.Elapsed().InMilliseconds();
  if (elapsed_time_ms > slowest_serialization_ms_) {
    last_ukm_source_id_ = document.GetUkmSourceId();
    last_ukm_url_ = document.CanonicalUrlForSharing().GetString().Utf8();
    slowest_serialization_ms_ = elapsed_time_ms;
  }

  if (ukm_timer_->Elapsed().InSeconds() >= kMinUKMDelayInSeconds)
    MaybeSendUKM();
}

void RenderAccessibilityImpl::SendLocationChanges() {
  TRACE_EVENT0("accessibility", "RenderAccessibilityImpl::SendLocationChanges");

  std::vector<mojom::LocationChangesPtr> changes;

  // Update layout on the root of the tree.
  WebAXObject root = tree_source_->GetRoot();

  // TODO(accessibility) Change this to CheckValidity() if there aren't crash
  // reports of illegal lifecycle changes from WebDisallowTransitionScope.
  if (!root.MaybeUpdateLayoutAndCheckValidity())
    return;

  blink::WebVector<WebAXObject> changed_bounds_objects;
  root.GetAllObjectsWithChangedBounds(changed_bounds_objects);
  for (const WebAXObject& obj : changed_bounds_objects) {
    // See if we had a previous location. If not, this whole subtree must
    // be new, so no need to update.
    int id = obj.AxID();
    if (!tree_source_->HasCachedBoundingBox(id))
      continue;

    // If the location has changed, append it to the IPC message.
    ui::AXRelativeBounds new_location;
    tree_source_->PopulateAXRelativeBounds(obj, &new_location);
    if (new_location != tree_source_->GetCachedBoundingBox(id))
      changes.push_back(mojom::LocationChanges::New(id, new_location));

    // Save the new location.
    tree_source_->SetCachedBoundingBox(id, new_location);
  }

  if (changes.empty())
    return;

  // Ensure that the number of cached bounding boxes doesn't exceed the
  // number of nodes in the tree, that would indicate the cache could
  // grow without bounds. Calls from the serializer to
  // BlinkAXTreeSerializer::SerializerClearedNode are supposed to keep the
  // cache trimmed to only actual nodes in the tree.
  DCHECK_LE(tree_source_->GetCachedBoundingBoxCount(),
            serializer_->ClientTreeNodeCount());

  render_accessibility_manager_->HandleLocationChanges(std::move(changes));
}

void RenderAccessibilityImpl::OnAccessibilityEventsHandled() {
  DCHECK_EQ(event_schedule_status_, EventScheduleStatus::kWaitingForAck);
  event_schedule_status_ = EventScheduleStatus::kNotWaiting;
  switch (event_schedule_mode_) {
    case EventScheduleMode::kDeferEvents:
      ScheduleSendPendingAccessibilityEvents(true);
      break;
    case EventScheduleMode::kProcessEventsImmediately:
      SendPendingAccessibilityEvents();
      break;
  }
}

void RenderAccessibilityImpl::OnLoadInlineTextBoxes(
    const ui::AXActionTarget* target) {
  const BlinkAXActionTarget* blink_target =
      BlinkAXActionTarget::FromAXActionTarget(target);
  if (!blink_target)
    return;
  const WebAXObject& obj = blink_target->WebAXObject();

  ScopedFreezeBlinkAXTreeSource freeze(tree_source_.get());
  if (tree_source_->ShouldLoadInlineTextBoxes(obj))
    return;

  tree_source_->SetLoadInlineTextBoxesForId(obj.AxID());

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  // This object may not be a leaf node. Force the whole subtree to be
  // re-serialized.
  serializer_->InvalidateSubtree(obj);

  // Explicitly send a tree change update event now.
  HandleAXEvent(ui::AXEvent(obj.AxID(), ax::mojom::Event::kTreeChanged));
}

void RenderAccessibilityImpl::OnGetImageData(const ui::AXActionTarget* target,
                                             const gfx::Size& max_size) {
  const BlinkAXActionTarget* blink_target =
      BlinkAXActionTarget::FromAXActionTarget(target);
  if (!blink_target)
    return;
  const WebAXObject& obj = blink_target->WebAXObject();

  ScopedFreezeBlinkAXTreeSource freeze(tree_source_.get());
  if (tree_source_->image_data_node_id() == obj.AxID())
    return;

  tree_source_->set_image_data_node_id(obj.AxID());
  tree_source_->set_max_image_data_size(max_size);

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  serializer_->InvalidateSubtree(obj);
  HandleAXEvent(ui::AXEvent(obj.AxID(), ax::mojom::Event::kImageFrameUpdated));
}

void RenderAccessibilityImpl::OnDestruct() {
  render_frame_ = nullptr;
  delete this;
}

void RenderAccessibilityImpl::AddPluginTreeToUpdate(
    ui::AXTreeUpdate* update,
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
  render_frame_->GetBrowserInterfaceBroker()->GetInterface(
      annotator.InitWithNewPipeAndPassReceiver());

  ax_image_annotator_ =
      std::make_unique<AXImageAnnotator>(this, std::move(annotator));
  tree_source_->AddImageAnnotator(ax_image_annotator_.get());
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
    tree_source_->RemoveImageAnnotator();
    ax_image_annotator_->Destroy();
    ax_image_annotator_.release();
  }
}

void RenderAccessibilityImpl::MarkAllAXObjectsDirty(ax::mojom::Role role) {
  ScopedFreezeBlinkAXTreeSource freeze(tree_source_.get());
  base::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(tree_source_->GetRoot());
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    if (obj.Role() == role)
      MarkWebAXObjectDirty(obj, /* subtree */ false);

    std::vector<blink::WebAXObject> children;
    tree_source_->GetChildren(obj, &children);
    for (WebAXObject& child : children)
      objs_to_explore.push(child);
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

void RenderAccessibilityImpl::AddImageAnnotationDebuggingAttributes(
    const std::vector<ui::AXTreeUpdate>& updates) {
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

WebAXObject RenderAccessibilityImpl::GetPluginRoot() {
  ScopedFreezeBlinkAXTreeSource freeze(tree_source_.get());
  WebAXObject root = tree_source_->GetRoot();
  if (!root.MaybeUpdateLayoutAndCheckValidity())
    return WebAXObject();

  base::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(root);
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    WebNode node = obj.GetNode();
    if (!node.IsNull() && node.IsElementNode()) {
      WebElement element = node.To<WebElement>();
      if (element.HasHTMLTagName("embed")) {
        return obj;
      }
    }

    // Explore children of this object.
    std::vector<WebAXObject> children;
    tree_source_->GetChildren(obj, &children);
    for (const auto& child : children)
      objs_to_explore.push(child);
  }

  return WebAXObject();
}

void RenderAccessibilityImpl::CancelScheduledEvents() {
  switch (event_schedule_status_) {
    case EventScheduleStatus::kScheduledDeferred:
    case EventScheduleStatus::kScheduledImmediate:  // Fallthrough
      weak_factory_for_pending_events_.InvalidateWeakPtrs();
      event_schedule_status_ = EventScheduleStatus::kNotWaiting;
      break;
    case EventScheduleStatus::kWaitingForAck:
    case EventScheduleStatus::kNotWaiting:  // Fallthrough
      break;
  }
}

void RenderAccessibilityImpl::MaybeSendUKM() {
  if (slowest_serialization_ms_ < kMinSerializationTimeToSendInMS)
    return;

  ukm::builders::Accessibility_Renderer(last_ukm_source_id_)
      .SetCpuTime_SendPendingAccessibilityEvents(slowest_serialization_ms_)
      .Record(ukm_recorder_.get());
  ResetUKMData();
}

void RenderAccessibilityImpl::ResetUKMData() {
  slowest_serialization_ms_ = 0;
  ukm_timer_ = std::make_unique<base::ElapsedTimer>();
  last_ukm_source_id_ = ukm::kInvalidSourceId;
  last_ukm_url_ = "";
}

RenderAccessibilityImpl::DirtyObject::DirtyObject() = default;
RenderAccessibilityImpl::DirtyObject::DirtyObject(const DirtyObject& other) =
    default;
RenderAccessibilityImpl::DirtyObject::~DirtyObject() = default;

}  // namespace content
