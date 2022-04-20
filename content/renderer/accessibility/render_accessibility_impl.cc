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
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/accessibility/ax_action_target_factory.h"
#include "content/renderer/accessibility/ax_image_annotator.h"
#include "content/renderer/accessibility/ax_tree_snapshotter_impl.h"
#include "content/renderer/accessibility/blink_ax_action_target.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_disallow_transition_scope.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_event_intent.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using blink::WebAXContext;
using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebSettings;
using blink::WebView;

namespace {

// The minimum amount of time that should be spent in serializing code in order
// to report the elapsed time as a URL-keyed metric.
constexpr base::TimeDelta kMinSerializationTimeToSend = base::Milliseconds(100);

// When URL-keyed metrics for the amount of time spent in serializing code
// are sent, the minimum amount of time to wait, in seconds, before
// sending metrics. Metrics may also be sent once per page transition.
constexpr base::TimeDelta kMinUKMDelay = base::Seconds(300);

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
  static auto* const ax_mode_crash_key = base::debug::AllocateCrashKeyString(
      "ax_mode", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(ax_mode_crash_key, mode.ToString());
}

}  // namespace

namespace content {

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
      event_schedule_status_(EventScheduleStatus::kNotWaiting),
      reset_token_(0),
      ukm_timer_(std::make_unique<base::ElapsedTimer>()),
      last_ukm_source_id_(ukm::kInvalidSourceId) {
  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
  content::RenderThread::Get()->BindHostReceiver(
      recorder.InitWithNewPipeAndPassReceiver());
  ukm_recorder_ = std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
  WebView* web_view = render_frame_->GetWebView();
  WebSettings* settings = web_view->GetSettings();

  SetAccessibilityCrashKey(mode);
#if BUILDFLAG(IS_ANDROID)
  // Password values are only passed through on Android.
  settings->SetAccessibilityPasswordValuesEnabled(true);
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Inline text boxes can be enabled globally on all except Android.
  // On Android they can be requested for just a specific node.
  if (mode.has_mode(ui::AXMode::kInlineTextBoxes))
    settings->SetInlineTextBoxAccessibilityEnabled(true);
#endif

#if BUILDFLAG(IS_MAC)
  // aria-modal currently prunes the accessibility tree on Mac only.
  settings->SetAriaModalPrunesAXTree(true);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Do not ignore SVG grouping (<g>) elements on ChromeOS, which is needed so
  // Select-to-Speak can read SVG text nodes in natural reading order.
  settings->SetAccessibilityIncludeSvgGElement(true);
#endif

  event_schedule_mode_ = EventScheduleMode::kDeferEvents;

  // Optionally disable AXMenuList, which makes the internal pop-up menu
  // UI for a select element directly accessible. Disable by default on
  // Chrome OS, but some tests may override.
  bool disable_ax_menu_list = false;
#if BUILDFLAG(IS_CHROMEOS)
  disable_ax_menu_list = true;
#endif
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kDisableAXMenuList)) {
    disable_ax_menu_list = command_line->GetSwitchValueASCII(
                               ::switches::kDisableAXMenuList) != "false";
  }
  if (disable_ax_menu_list)
    settings->SetUseAXMenuList(false);

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    ax_context_ = std::make_unique<WebAXContext>(document, mode);
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
    ax_context_ =
        std::make_unique<WebAXContext>(document, GetAccessibilityMode());
}

void RenderAccessibilityImpl::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  has_injected_stylesheet_ = false;

  // If we have events scheduled, but not sent, cancel them
  CancelScheduledEvents();
  // Defer events during initial page load.
  event_schedule_mode_ = EventScheduleMode::kDeferEvents;

  MaybeSendUKM();
  slowest_serialization_time_ = base::TimeDelta();
  ukm_timer_ = std::make_unique<base::ElapsedTimer>();

  // Remove the image annotator if the page is loading and it was added for
  // the one-shot image annotation (i.e. AXMode for image annotation is not
  // set).
  if (!ax_image_annotator_ ||
      GetAccessibilityMode().has_mode(ui::AXMode::kLabelImages)) {
    return;
  }
  tree_source_->RemoveBlinkImageAnnotator();
  ax_image_annotator_->Destroy();
  ax_image_annotator_.reset();
  page_language_.clear();
}

void RenderAccessibilityImpl::AccessibilityModeChanged(const ui::AXMode& mode) {
  ui::AXMode old_mode = GetAccessibilityMode();
  if (old_mode == mode)
    return;
  tree_source_->SetAccessibilityMode(mode);

  SetAccessibilityCrashKey(mode);

#if !BUILDFLAG(IS_ANDROID)
  // Inline text boxes can be enabled globally on all except Android.
  // On Android they can be requested for just a specific node.
  WebView* web_view = render_frame_->GetWebView();
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
#endif  // !BUILDFLAG(IS_ANDROID)

  serializer_->Reset();
  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    StartOrStopLabelingImages(old_mode, mode);

    needs_initial_ax_tree_root_ = true;
    event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;
    ScheduleSendPendingAccessibilityEvents();
  }
}

// This function expects the |point| passed by parameter to be relative to the
// page viewport, always. This means that when the position is within a popup,
// the |point| should still be relative to the web page's viewport.
void RenderAccessibilityImpl::HitTest(
    const gfx::Point& point,
    ax::mojom::Event event_to_fire,
    int request_id,
    mojom::RenderAccessibility::HitTestCallback callback) {
  WebAXObject ax_object;

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    auto root_obj = WebAXObject::FromWebDocument(document);
    if (root_obj.MaybeUpdateLayoutAndCheckValidity()) {
      // 1. Now that layout has been updated for the entire document, try to run
      // the hit test operation on the popup root element, if there's a popup
      // opened. This is needed to allow hit testing within web content popups.
      absl::optional<gfx::RectF> popup_bounds = GetPopupBounds();
      if (popup_bounds.has_value()) {
        auto popup_root_obj = WebAXObject::FromWebDocument(GetPopupDocument());
        // WebAXObject::HitTest expects the point passed by parameter to be
        // relative to the instance we call it from.
        ax_object = popup_root_obj.HitTest(
            point - ToRoundedVector2d(popup_bounds->OffsetFromOrigin()));
      }

      // 2. If running the hit test operation on the popup didn't returned any
      // result (or if there was no popup), run the hit test operation from the
      // main element.
      if (ax_object.IsNull())
        ax_object = root_obj.HitTest(point);
    }
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
      HandleAXEvent(ui::AXEvent(
          ax_object.AxID(), event_to_fire, ax::mojom::EventFrom::kAction,
          ax::mojom::Action::kHitTest, intents, request_id));
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
    gfx::Rect rect = ax_object.GetBoundsInFrameCoordinates();
    // The following transformation of the input point is naive, but works
    // fairly well. It will fail with CSS transforms that rotate or shear.
    // https://crbug.com/981959.
    WebView* web_view = render_frame_->GetWebView();
    gfx::PointF viewport_offset = web_view->VisualViewportOffset();
    transformed_point +=
        gfx::Vector2d(viewport_offset.x(), viewport_offset.y()) -
        rect.OffsetFromOrigin();
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

  if (target->PerformAction(data))
    return;

  if (!root.MaybeUpdateLayoutAndCheckValidity())
    return;

  switch (data.action) {
    case ax::mojom::Action::kBlur: {
      ui::AXActionData action_data;
      action_data.action = ax::mojom::Action::kFocus;
      root.PerformAction(action_data);
      break;
    }
    case ax::mojom::Action::kGetImageData:
      OnGetImageData(target.get(), data.target_rect.size());
      break;
    case ax::mojom::Action::kLoadInlineTextBoxes:
      OnLoadInlineTextBoxes(target.get());
      break;
    case ax::mojom::Action::kSetSelection:
      anchor->SetSelection(anchor.get(), data.anchor_offset, focus.get(),
                           data.focus_offset);
      break;
    case ax::mojom::Action::kScrollToMakeVisible:
      target->ScrollToMakeVisibleWithSubFocus(
          data.target_rect, data.horizontal_scroll_alignment,
          data.vertical_scroll_alignment, data.scroll_behavior);
      break;
    case ax::mojom::Action::kClearAccessibilityFocus:
    case ax::mojom::Action::kDecrement:
    case ax::mojom::Action::kDoDefault:
    case ax::mojom::Action::kIncrement:
    case ax::mojom::Action::kScrollToPoint:
    case ax::mojom::Action::kFocus:
    case ax::mojom::Action::kSetAccessibilityFocus:
    case ax::mojom::Action::kSetScrollOffset:
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
    case ax::mojom::Action::kSetValue:
    case ax::mojom::Action::kShowContextMenu:
      // These are all handled by PerformAction.
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
        MarkAllAXObjectsDirty(ax::mojom::Role::kImage,
                              ax::mojom::Action::kAnnotatePageImages);
      }
      break;
    case ax::mojom::Action::kSignalEndOfTest:
      // Wait for 100ms to allow pending events to come in
      base::PlatformThread::Sleep(base::Milliseconds(100));

      HandleAXEvent(ui::AXEvent(root.AxID(), ax::mojom::Event::kEndOfTest));
      break;
    case ax::mojom::Action::kShowTooltip:
    case ax::mojom::Action::kHideTooltip:
    case ax::mojom::Action::kInternalInvalidateTree:
    case ax::mojom::Action::kResumeMedia:
    case ax::mojom::Action::kStartDuckingMedia:
    case ax::mojom::Action::kStopDuckingMedia:
    case ax::mojom::Action::kSuspendMedia:
      break;
  }
}

void RenderAccessibilityImpl::Reset(int32_t reset_token) {
  reset_token_ = reset_token;
  serializer_->Reset();
  pending_events_.clear();
  dirty_objects_.clear();

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    // Tree-only mode gets used by the automation extension API which requires a
    // load complete event to invoke listener callbacks.
    // SendPendingAccessibilityEvents() will fire the load complete event
    // if the page is loaded.
    needs_initial_ax_tree_root_ = true;
    event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;
    ScheduleSendPendingAccessibilityEvents();
  }
}

void RenderAccessibilityImpl::MarkWebAXObjectDirty(
    const WebAXObject& obj,
    bool subtree,
    ax::mojom::EventFrom event_from,
    ax::mojom::Action event_from_action,
    std::vector<ui::AXEventIntent> event_intents,
    ax::mojom::Event event_type) {
  DCHECK(obj.AccessibilityIsIncludedInTree())
      << "Cannot serialize unincluded object: " << obj.ToString(true).Utf8();
  EnqueueDirtyObject(obj, event_from, event_from_action, event_intents,
                     dirty_objects_.end());

  if (subtree)
    serializer_->InvalidateSubtree(obj);

  // If the event occurred on the focused object, process immediately.
  // kLayoutComplete is an exception because it always fires on the root
  // object but it doesn't imply immediate processing is needed.
  if (obj.IsFocused() && event_type != ax::mojom::Event::kLayoutComplete)
    event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;

  ScheduleSendPendingAccessibilityEvents();
}

void RenderAccessibilityImpl::HandleAXEvent(const ui::AXEvent& event) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  auto obj = WebAXObject::FromWebDocumentByID(document, event.id);
  if (obj.IsDetached())
    return;

#if BUILDFLAG(IS_ANDROID)
  // Inline text boxes are needed to support moving by character/word/line.
  // On Android, we don't load inline text boxes by default, only on-demand, or
  // when part of the focused object. So, when focus moves to an editable text
  // field, ensure we re-serialize the whole thing including its inline text
  // boxes.
  if (event.event_type == ax::mojom::Event::kFocus && obj.IsEditable())
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
  if (IsImmediateProcessingRequiredForEvent(event))
    event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;

  if (ShouldSerializeNodeForEvent(obj, event)) {
    MarkWebAXObjectDirty(obj, /* subtree= */ false, event.event_from,
                         event.event_from_action, event.event_intents,
                         event.event_type);
  }

  ScheduleSendPendingAccessibilityEvents();
}

bool RenderAccessibilityImpl::IsImmediateProcessingRequiredForEvent(
    const ui::AXEvent& event) const {
  if (event_schedule_mode_ == EventScheduleMode::kProcessEventsImmediately)
    return true;  // Already scheduled for immediate mode.

  if (event.event_from == ax::mojom::EventFrom::kAction)
    return true;  // Actions should result in an immediate response.

  switch (event.event_type) {
    case ax::mojom::Event::kActiveDescendantChanged:
    case ax::mojom::Event::kBlur:
    case ax::mojom::Event::kCheckedStateChanged:
    case ax::mojom::Event::kClicked:
    case ax::mojom::Event::kDocumentSelectionChanged:
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kHover:
    case ax::mojom::Event::kLoadComplete:
    case ax::mojom::Event::kTextSelectionChanged:
    case ax::mojom::Event::kValueChanged:
      return true;

    case ax::mojom::Event::kAriaAttributeChanged:
    case ax::mojom::Event::kChildrenChanged:
    case ax::mojom::Event::kDocumentTitleChanged:
    case ax::mojom::Event::kExpandedChanged:
    case ax::mojom::Event::kHide:
    case ax::mojom::Event::kLayoutComplete:
    case ax::mojom::Event::kLoadStart:
    case ax::mojom::Event::kLocationChanged:
    case ax::mojom::Event::kMenuListValueChanged:
    case ax::mojom::Event::kRowCollapsed:
    case ax::mojom::Event::kRowCountChanged:
    case ax::mojom::Event::kRowExpanded:
    case ax::mojom::Event::kScrollPositionChanged:
    case ax::mojom::Event::kScrolledToAnchor:
    case ax::mojom::Event::kSelectedChildrenChanged:
    case ax::mojom::Event::kShow:
    case ax::mojom::Event::kTextChanged:
      return false;

    case ax::mojom::Event::kAlert:
    case ax::mojom::Event::kAutocorrectionOccured:
    case ax::mojom::Event::kControlsChanged:
    case ax::mojom::Event::kEndOfTest:
    case ax::mojom::Event::kFocusAfterMenuClose:
    case ax::mojom::Event::kFocusContext:
    case ax::mojom::Event::kHitTestResult:
    case ax::mojom::Event::kImageFrameUpdated:
    case ax::mojom::Event::kLiveRegionCreated:
    case ax::mojom::Event::kLiveRegionChanged:
    case ax::mojom::Event::kMediaStartedPlaying:
    case ax::mojom::Event::kMediaStoppedPlaying:
    case ax::mojom::Event::kMenuEnd:
    case ax::mojom::Event::kMenuPopupEnd:
    case ax::mojom::Event::kMenuPopupStart:
    case ax::mojom::Event::kMenuStart:
    case ax::mojom::Event::kMouseCanceled:
    case ax::mojom::Event::kMouseDragged:
    case ax::mojom::Event::kMouseMoved:
    case ax::mojom::Event::kMousePressed:
    case ax::mojom::Event::kMouseReleased:
    case ax::mojom::Event::kNone:
    case ax::mojom::Event::kSelection:
    case ax::mojom::Event::kSelectionAdd:
    case ax::mojom::Event::kSelectionRemove:
    case ax::mojom::Event::kStateChanged:
    case ax::mojom::Event::kTooltipClosed:
    case ax::mojom::Event::kTooltipOpened:
    case ax::mojom::Event::kTreeChanged:
    case ax::mojom::Event::kWindowActivated:
    case ax::mojom::Event::kWindowDeactivated:
    case ax::mojom::Event::kWindowVisibilityChanged:
      // Never fired from Blink.
      NOTREACHED() << "Event not expected from Blink: " << event.event_type;
      return false;
  }
}

bool RenderAccessibilityImpl::ShouldSerializeNodeForEvent(
    const WebAXObject& obj,
    const ui::AXEvent& event) const {
  if (obj.IsDetached())
    return false;

  if (event.event_type == ax::mojom::Event::kTextSelectionChanged &&
      !obj.IsAtomicTextField()) {
    // Selection changes on non-atomic text fields cause no change to the
    // control node's data.
    //
    // Selection offsets exposed via kTextSelStart and kTextSelEnd are only used
    // for atomic text fields, (input of a text field type, and textarea). Rich
    // editable areas, such as contenteditables, use AXTreeData.
    //
    // TODO(nektar): Remove kTextSelStart and kTextSelEnd from the renderer.
    return false;
  }

  return true;
}

std::list<std::unique_ptr<AXDirtyObject>>::iterator
RenderAccessibilityImpl::EnqueueDirtyObject(
    const blink::WebAXObject& obj,
    ax::mojom::EventFrom event_from,
    ax::mojom::Action event_from_action,
    std::vector<ui::AXEventIntent> event_intents,
    std::list<std::unique_ptr<AXDirtyObject>>::iterator insertion_point) {
  AXDirtyObject* dirty_object = new AXDirtyObject();
  dirty_object->obj = obj;
  dirty_object->event_from = event_from;
  dirty_object->event_from_action = event_from_action;
  dirty_object->event_intents = event_intents;
  return std::next(dirty_objects_.insert(
      insertion_point, base::WrapUnique<AXDirtyObject>(dirty_object)));
}

int RenderAccessibilityImpl::GetDeferredEventsDelay() {
  // The amount of time, in milliseconds, to wait before sending non-interactive
  // events that are deferred before the initial page load.
  constexpr int kDelayForDeferredUpdatesBeforePageLoad = 350;

  // The amount of time, in milliseconds, to wait before sending non-interactive
  // events that are deferred after the initial page load.
  // Shync with same constant in CrossPlatformAccessibilityBrowserTest.
  constexpr int kDelayForDeferredUpdatesAfterPageLoad = 150;

  // Prefer WebDocument::IsLoaded() over WebAXObject::IsLoaded() as the
  // latter could trigger a layout update while retrieving the root
  // WebAXObject.
  return GetMainDocument().IsLoaded() ? kDelayForDeferredUpdatesAfterPageLoad
                                      : kDelayForDeferredUpdatesBeforePageLoad;
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

  // Don't send accessibility events for frames that don't yet have an tree id
  // as doing so will cause the browser to discard that message and all
  // subsequent ones.
  // TODO(1231184): There are some cases where no content is currently rendered,
  // due to an iframe returning 204 or window.stop() being called. In these
  // cases there will never be an AXTreeID as there is no commit, which will
  // prevent accessibility updates from ever being sent even if the rendering is
  // fixed.
  if (!render_frame_->GetWebFrame()->GetAXTreeID().token())
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

  base::TimeDelta delay = base::TimeDelta();
  switch (event_schedule_mode_) {
    case EventScheduleMode::kDeferEvents:
      event_schedule_status_ = EventScheduleStatus::kScheduledDeferred;
      // Where the user is not currently navigating or typing,
      // process changes on a delay so that they occur in larger batches,
      // improving efficiency of repetitive mutations.
      delay = base::Milliseconds(GetDeferredEventsDelay());
      break;
    case EventScheduleMode::kProcessEventsImmediately:
      // This set of events needed to be processed immediately because of a
      // page load or user action.
      event_schedule_status_ = EventScheduleStatus::kScheduledImmediate;
      delay = base::TimeDelta();
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
  plugin_serializer_ =
      std::make_unique<PluginAXTreeSerializer>(plugin_tree_source_);

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
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kShowContextMenu;
  target->PerformAction(action_data);
}

WebDocument RenderAccessibilityImpl::GetMainDocument() {
  if (render_frame_ && render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->GetDocument();
  return WebDocument();
}

std::string RenderAccessibilityImpl::GetLanguage() {
  return page_language_;
}

bool RenderAccessibilityImpl::SerializeUpdatesAndEvents(
    WebDocument document,
    WebAXObject root,
    std::vector<ui::AXEvent>& events,
    std::vector<ui::AXTreeUpdate>& updates,
    bool invalidate_plugin_subtree) {
  // Make a copy of the events, because it's possible that
  // actions inside this loop will cause more events to be
  // queued up.

  std::vector<ui::AXEvent> src_events = pending_events_;
  pending_events_.clear();

  bool had_end_of_test_event = false;

  // If there's a layout complete or a scroll changed message, we need to send
  // location changes.
  bool need_to_send_location_changes = false;

  // Keep track of load complete messages. When a load completes, it's a good
  // time to inject a stylesheet for image annotation debugging.
  bool had_load_complete_messages = false;

  // Loop over each event and generate an updated event message.
  for (ui::AXEvent& event : src_events) {
    if (event.event_type == ax::mojom::Event::kLayoutComplete)
      need_to_send_location_changes = true;

    if (event.event_type == ax::mojom::Event::kLoadComplete)
      had_load_complete_messages = true;

    if (event.event_type == ax::mojom::Event::kEndOfTest) {
      had_end_of_test_event = true;
      continue;
    }

    events.push_back(event);

    VLOG(1) << "Accessibility event: " << ui::ToString(event.event_type)
            << " on node id " << event.id;
  }

  // Dirty objects can be added as a result of serialization. For example,
  // as children are iterated during depth first traversal in the serializer,
  // the children sometimes need to be created. The initialization of these
  // new children can lead to the discovery of parenting changes via
  // aria-owns, or name changes on an ancestor that collects its name its from
  // contents. In some cases this has led to an infinite loop, as the
  // serialization of new dirty objects keeps adding new dirty objects to
  // consider. The infinite loop is avoided by tracking the number of dirty
  // objects that can be serialized from the loop, which is the initial
  // number of dirty objects + kMaxExtraDirtyObjectsToSerialize.
  // Allowing kMaxExtraDirtyObjectsToSerialize ensures that most important
  // additional related changes occur at the same time, and that dump event
  // tests have consistent results (the results change when dirty objects are
  // processed in separate batches).
  constexpr int kMaxExtraDirtyObjectsToSerialize = 100;
  size_t num_remaining_objects_to_serialize =
      dirty_objects_.size() + kMaxExtraDirtyObjectsToSerialize;

  // Keep track of IDs serialized so we don't serialize the same node twice.
  std::set<int32_t> already_serialized_ids;

  // Serialize all dirty objects in the list at this point in time, stopping
  // either when the queue is empty, or the number of remaining objects to
  // serialize has been reached.
  while (!dirty_objects_.empty() && --num_remaining_objects_to_serialize > 0) {
    std::unique_ptr<AXDirtyObject> current_dirty_object =
        std::move(dirty_objects_.front());
    dirty_objects_.pop_front();
    auto obj = current_dirty_object->obj;

    // Dirty objects can be added using MarkWebAXObjectDirty(obj) from other
    // parts of the code as well, so we need to ensure the object still exists.
    // TODO(accessibility) Change this to CheckValidity() if there aren't crash
    // reports of illegal lifecycle changes from WebDisallowTransitionScope.
    if (!obj.MaybeUpdateLayoutAndCheckValidity())
      continue;

    // Cannot serialize unincluded object.
    // Only included objects are marked dirty, but this can happen if the object
    // becomes unincluded after it was originally marked dirty, in which case a
    // children changed will also be fired on the included ancestor. The
    // children changed event on the ancestor means that attempting to serialize
    // this unincluded object is not necessary.
    if (!obj.AccessibilityIsIncludedInTree())
      continue;

    // Further down this loop, we update |already_serialized_ids| with all IDs
    // actually serialized. However, add this object's ID first because there's
    // a chance that we try to serialize this object but the serializer ends up
    // skipping it. That's probably a Blink bug if that happens, but still we
    // need to make sure we don't keep trying the same object over again.
    if (!already_serialized_ids.insert(obj.AxID()).second)
      continue;  // No insertion, was already present.

    ui::AXTreeUpdate update;
    update.event_from = current_dirty_object->event_from;
    update.event_from_action = current_dirty_object->event_from_action;
    update.event_intents = current_dirty_object->event_intents;
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

  if (had_end_of_test_event) {
    ui::AXEvent end_of_test(root.AxID(), ax::mojom::Event::kEndOfTest);
    if (!WebAXObject::IsDirty(document)) {
      events.emplace_back(end_of_test);
    } else {
      // Document is still dirty, queue up another end of test and process
      // immediately.
      event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;
      HandleAXEvent(end_of_test);
    }
  }

  if (had_load_complete_messages) {
    has_injected_stylesheet_ = false;
  }

  return need_to_send_location_changes;
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

  DCHECK(document.IsAccessibilityEnabled())
      << "SendPendingAccessibilityEvents should not do any work when nothing "
         "has enabled accessibility.";

  if (needs_initial_ax_tree_root_) {
    // At the very start of accessibility for this document, push a layout
    // complete for the entire document, in order to initialize the browser's
    // cached accessibility tree.
    needs_initial_ax_tree_root_ = false;
    auto root_obj = WebAXObject::FromWebDocument(document);
    // Always fire layout complete for a new root object.
    pending_events_.insert(
        pending_events_.begin(),
        ui::AXEvent(root_obj.AxID(), ax::mojom::Event::kLayoutComplete));
    MarkWebAXObjectDirty(root_obj, false);

    // If loaded and has some content, insert load complete at the top, so that
    // screen readers are informed a new document is ready.
    if (root_obj.IsLoaded() && !document.Body().IsNull() &&
        !document.Body().FirstChild().IsNull()) {
      pending_events_.insert(
          pending_events_.begin(),
          ui::AXEvent(root_obj.AxID(), ax::mojom::Event::kLoadComplete));
    }
  }

  if (pending_events_.empty() && dirty_objects_.empty()) {
    // By default, assume the next batch does not have interactive events, and
    // defer so that the batch of events is larger. If any interactive events
    // come in, the batch will be processed immediately.
    event_schedule_mode_ = EventScheduleMode::kDeferEvents;
    return;
  }

  // Update layout before snapshotting the events so that live state read from
  // the DOM during freezing (e.g. which node currently has focus) is consistent
  // with the events and node data we're about to send up.
  WebAXObject::UpdateLayout(document);

  ScopedFreezeBlinkAXTreeSource freeze(tree_source_.get());

  WebAXObject root = tree_source_->GetRoot();
#if DCHECK_IS_ON()
  // Never causes a document lifecycle change during serialization,
  // because the assumption is that layout is in a safe, stable state.
  // (Skip if image_annotation_debugging_ is enabled because it adds
  // style attributes to images, affecting the document lifecycle
  // during accessibility.)
  std::unique_ptr<blink::WebDisallowTransitionScope> disallow;
  if (!image_annotation_debugging_)
    disallow = std::make_unique<blink::WebDisallowTransitionScope>(&document);
#endif

  // Save the page language.
  page_language_ = root.Language().Utf8();

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
  std::unique_ptr<blink::WebDisallowTransitionScope> disallow2;
  if (!image_annotation_debugging_) {
    disallow = std::make_unique<blink::WebDisallowTransitionScope>(
        &popup_or_main_document);
  }
#endif

  // Keep track of if the host node for a plugin has been invalidated,
  // because if so, the plugin subtree will need to be re-serialized.
  bool invalidate_plugin_subtree = false;
  if (plugin_tree_source_ && !plugin_host_node_.IsDetached()) {
    invalidate_plugin_subtree = !serializer_->IsInClientTree(plugin_host_node_);
  }

  // The serialized list of updates and events to send to the browser.
  mojom::AXUpdatesAndEventsPtr updates_and_events =
      mojom::AXUpdatesAndEvents::New();

  bool need_to_send_location_changes = SerializeUpdatesAndEvents(
      document, root, updates_and_events->events, updates_and_events->updates,
      invalidate_plugin_subtree);

  if (image_annotation_debugging_)
    AddImageAnnotationDebuggingAttributes(updates_and_events->updates);

  event_schedule_status_ = EventScheduleStatus::kWaitingForAck;
  render_accessibility_manager_->HandleAccessibilityEvents(
      std::move(updates_and_events), reset_token_,
      base::BindOnce(&RenderAccessibilityImpl::OnAccessibilityEventsHandled,
                     weak_factory_for_pending_events_.GetWeakPtr()));
  reset_token_ = 0;

  if (need_to_send_location_changes)
    SendLocationChanges();

  // Now that this batch is complete, assume the next batch does not have
  // interactive events, and defer so that the batch of events is larger.
  // If any interactive events come in, the batch will be processed immediately.
  event_schedule_mode_ = EventScheduleMode::kDeferEvents;


  // Measure the amount of time spent in this function. Keep track of the
  // maximum within a time interval so we can upload UKM.
  base::TimeDelta elapsed_time_ms = timer.Elapsed();
  if (elapsed_time_ms > slowest_serialization_time_) {
    last_ukm_source_id_ = document.GetUkmSourceId();
    slowest_serialization_time_ = elapsed_time_ms;
  }
  // Also log the time taken in this function to track serialization
  // performance.
  UMA_HISTOGRAM_TIMES(
      "Accessibility.Performance.SendPendingAccessibilityEvents",
      elapsed_time_ms);

  if (ukm_timer_->Elapsed() >= kMinUKMDelay)
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
  event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;
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
  event_schedule_mode_ = EventScheduleMode::kProcessEventsImmediately;
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

  for (ui::AXNodeData& node : update->nodes) {
    if (node.role == ax::mojom::Role::kEmbeddedObject) {
      plugin_host_node_ = WebAXObject::FromWebDocumentByID(document, node.id);

      const ui::AXNode* root = plugin_tree_source_->GetRoot();
      node.child_ids.push_back(root->id());

      ui::AXTreeUpdate plugin_update;
      plugin_serializer_->SerializeChanges(root, &plugin_update);

      size_t old_count = update->nodes.size();
      size_t new_count = plugin_update.nodes.size();
      update->nodes.resize(old_count + new_count);
      for (size_t i = 0; i < new_count; ++i)
        update->nodes[old_count + i] = plugin_update.nodes[i];
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
  tree_source_->AddBlinkImageAnnotator(ax_image_annotator_.get());
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
    tree_source_->RemoveBlinkImageAnnotator();
    ax_image_annotator_->Destroy();
    ax_image_annotator_.reset();
  }
}

void RenderAccessibilityImpl::MarkAllAXObjectsDirty(
    ax::mojom::Role role,
    ax::mojom::Action event_from_action) {
  ScopedFreezeBlinkAXTreeSource freeze(tree_source_.get());
  base::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(tree_source_->GetRoot());
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    if (obj.Role() == role)
      MarkWebAXObjectDirty(obj, /* subtree */ false,
                           ax::mojom::EventFrom::kNone, event_from_action);

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
  blink::WebPagePopup* popup = render_frame_->GetWebView()->GetPagePopup();
  if (popup)
    return popup->GetDocument();
  return WebDocument();
}

absl::optional<gfx::RectF> RenderAccessibilityImpl::GetPopupBounds() {
  const WebDocument& popup_document = GetPopupDocument();
  if (popup_document.IsNull())
    return absl::nullopt;

  auto obj = WebAXObject::FromWebDocument(popup_document);

  gfx::RectF popup_bounds;
  WebAXObject popup_container;
  gfx::Transform transform;
  obj.GetRelativeBounds(popup_container, popup_bounds, transform);

  // The |popup_container| will never be set for a popup element. See
  // `AXObject::GetRelativeBounds`.
  DCHECK(popup_container.IsNull());

  return popup_bounds;
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

void RenderAccessibilityImpl::ConnectionClosed() {
  event_schedule_status_ = EventScheduleStatus::kNotWaiting;
}

void RenderAccessibilityImpl::MaybeSendUKM() {
  if (slowest_serialization_time_ < kMinSerializationTimeToSend)
    return;

  ukm::builders::Accessibility_Renderer(last_ukm_source_id_)
      .SetCpuTime_SendPendingAccessibilityEvents(
          slowest_serialization_time_.InMilliseconds())
      .Record(ukm_recorder_.get());
  ResetUKMData();
}

void RenderAccessibilityImpl::ResetUKMData() {
  slowest_serialization_time_ = base::TimeDelta();
  ukm_timer_ = std::make_unique<base::ElapsedTimer>();
  last_ukm_source_id_ = ukm::kInvalidSourceId;
}

AXDirtyObject::AXDirtyObject() = default;
AXDirtyObject::AXDirtyObject(const AXDirtyObject& other) = default;
AXDirtyObject::~AXDirtyObject() = default;

}  // namespace content
