// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/accessibility/annotations/ax_annotators_manager.h"
#include "content/renderer/accessibility/ax_action_target_factory.h"
#include "content/renderer/accessibility/blink_ax_action_target.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_disallow_transition_scope.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_event_intent.h"
#include "ui/accessibility/ax_mode_histogram_logger.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using blink::WebAXContext;
using blink::WebAXObject;
using blink::WebDocument;
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
    RenderFrameImpl* const render_frame)
    : RenderFrameObserver(render_frame),
      render_accessibility_manager_(render_accessibility_manager),
      render_frame_(render_frame),
      ukm_timer_(std::make_unique<base::ElapsedTimer>()),
      last_ukm_source_id_(ukm::kInvalidSourceId) {
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);

#if BUILDFLAG(IS_ANDROID)
  // Password values are only passed through on Android.
  render_frame_->GetWebView()
      ->GetSettings()
      ->SetAccessibilityPasswordValuesEnabled(true);
#elif BUILDFLAG(IS_MAC)
  // aria-modal currently prunes the accessibility tree on Mac only.
  render_frame_->GetWebView()->GetSettings()->SetAriaModalPrunesAXTree(true);
#elif BUILDFLAG(IS_CHROMEOS)
  // Do not ignore SVG grouping (<g>) elements on ChromeOS, which is needed so
  // Select-to-Speak can read SVG text nodes in natural reading order.
  render_frame_->GetWebView()
      ->GetSettings()
      ->SetAccessibilityIncludeSvgGElement(true);
#endif

  ax_annotators_manager_ = std::make_unique<AXAnnotatorsManager>(this);
}

RenderAccessibilityImpl::~RenderAccessibilityImpl() = default;

void RenderAccessibilityImpl::DidCreateNewDocument() {
  const WebDocument& document = GetMainDocument();
  DCHECK(!document.IsNull());
  ax_context_ = std::make_unique<WebAXContext>(document, accessibility_mode_);

  // Set reset token which will be returned to browser in the next IPC, so that
  // RenderFrameHostImpl can discard stale data, when the token does not match
  // the expected token.
  ax_context_->SetSerializationResetToken(*reset_token_);

  ScheduleImmediateAXUpdate();
}

void RenderAccessibilityImpl::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  MaybeSendUKM();
  slowest_serialization_time_ = base::TimeDelta();
  ukm_timer_ = std::make_unique<base::ElapsedTimer>();

  ax_annotators_manager_->CancelAnnotations();
  page_language_.clear();

  // New document has started. Do not expect to receive the ACK for a
  // serialization sent by the old document.
  ax_context_->OnSerializationCancelled();
  weak_factory_for_pending_events_.InvalidateWeakPtrs();

  loading_stage_ = LoadingStage::kPreload;
}

void RenderAccessibilityImpl::NotifyAccessibilityModeChange(
    const ui::AXMode& mode) {
  CHECK(reset_token_);
  ui::AXMode old_mode = accessibility_mode_;
  DCHECK(!mode.is_mode_off())
      << "Should not be reached when turning a11y off; rather, the "
         "RenderAccessibilityImpl should be destroyed.";

  if (old_mode == mode) {
    DCHECK(ax_context_);
    NOTREACHED_IN_MIGRATION()
        << "Do not call AccessibilityModeChanged unless it changes.";
    return;
  }

  accessibility_mode_ = mode;

  bool was_on = !old_mode.is_mode_off();

  DCHECK_EQ(was_on, !!ax_context_);

  SetAccessibilityCrashKey(mode);

  if (ax_context_) {
    ax_context_->SetAXMode(mode);
  } else {
    DidCreateNewDocument();
  }

  DCHECK(ax_context_);
  DCHECK_EQ(accessibility_mode_, ax_context_->GetAXMode());

  // Log individual mode flags transitioning to the set state, as well as usage
  // of named bundles of node flags.
  ui::RecordAccessibilityModeHistograms(ui::AXHistogramPrefix::kBlink,
                                        accessibility_mode_, old_mode);

  // Build (or rebuild) the accessibility tree with the new mode.
  if (was_on) {
    ax_context_->MarkDocumentDirty();
  }

  // Update AXAnnotators based on a changed accessibility mode.
  ax_annotators_manager_->AccessibilityModeChanged(old_mode, mode);

  // Fire a load complete event so that any ATs present can treat the page as
  // fresh and newly loaded.
  FireLoadCompleteIfLoaded();
}

// Token to return this token in the next IPC, so that RenderFrameHostImpl
// can discard stale data, when the token does not match the expected token.
void RenderAccessibilityImpl::set_reset_token(uint32_t reset_token) {
  CHECK(reset_token);
  reset_token_ = reset_token;
  if (ax_context_) {
    ax_context_->SetSerializationResetToken(reset_token);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void RenderAccessibilityImpl::FireLayoutComplete() {
  if (ax_context_) {
    ax_context_->AddEventToSerializationQueue(
        ui::AXEvent(ComputeRoot().AxID(), ax::mojom::Event::kLayoutComplete),
        true);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void RenderAccessibilityImpl::FireLoadCompleteIfLoaded() {
  if (GetMainDocument().IsLoaded() &&
      GetMainDocument().GetFrame()->GetEmbeddingToken()) {
    DCHECK(ax_context_);
    ax_context_->FireLoadCompleteIfLoaded();
  }
}

// This function expects the |point| passed by parameter to be relative to the
// page viewport, always. This means that when the position is within a popup,
// the |point| should still be relative to the web page's viewport.
void RenderAccessibilityImpl::HitTest(
    const gfx::Point& point,
    ax::mojom::Event event_to_fire,
    int request_id,
    blink::mojom::RenderAccessibility::HitTestCallback callback) {
  const WebDocument& document = GetMainDocument();
  DCHECK(!document.IsNull());
  DCHECK(ax_context_);
  ax_context_->UpdateAXForAllDocuments();

  WebAXObject ax_object;
  auto root_obj = WebAXObject::FromWebDocument(document);
  ax_object = root_obj.HitTest(point);

  // Return if no attached accessibility object was found for the main document.
  if (ax_object.IsDetached()) {
    std::move(callback).Run(/*hit_test_response=*/nullptr);
    return;
  }

  DCHECK(ax_object.IsIncludedInTree());

  // If the result was in the same frame, return the result.
  ui::AXNodeData data;
  ax_object.Serialize(&data, ax_context_->GetAXMode());
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
      // Marking dirty ensures that a lifecycle update will be scheduled.
      MarkWebAXObjectDirty(ax_object);
      HandleAXEvent(ui::AXEvent(
          ax_object.AxID(), event_to_fire, ax::mojom::EventFrom::kAction,
          ax::mojom::Action::kHitTest, intents, request_id));
    }

    // Reply with the result.
    const auto& frame_token = render_frame_->GetWebFrame()->GetFrameToken();
    std::move(callback).Run(blink::mojom::HitTestResponse::New(
        frame_token, point, ax_object.AxID()));
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

  std::move(callback).Run(blink::mojom::HitTestResponse::New(
      child_frame->GetFrameToken(), transformed_point, ax_object.AxID()));
}

void RenderAccessibilityImpl::PerformAction(const ui::AXActionData& data) {
  if (!ax_context_) {
    return;
  }
  // Update layout and AX first before attempting to perform the action.
  ax_context_->UpdateAXForAllDocuments();

  WebDocument document = GetMainDocument();
  if (document.IsNull()) {
    return;
  }

  // TODO: think about how to handle this without holding onto a plugin tree
  // source.
  std::unique_ptr<ui::AXActionTarget> target =
      AXActionTargetFactory::CreateFromNodeIdOrRole(
          document, plugin_action_target_adapter_, data.target_node_id,
          data.target_role);
  std::unique_ptr<ui::AXActionTarget> anchor =
      AXActionTargetFactory::CreateFromNodeIdOrRole(
          document, plugin_action_target_adapter_, data.anchor_node_id);
  std::unique_ptr<ui::AXActionTarget> focus =
      AXActionTargetFactory::CreateFromNodeIdOrRole(
          document, plugin_action_target_adapter_, data.focus_node_id);

  ax_annotators_manager_->PerformAction(data.action);

  // Important: keep this reconciled with AXObject::PerformAction().
  // Actions shouldn't be handled in both places.
  switch (data.action) {
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
    case ax::mojom::Action::kBlur:
    case ax::mojom::Action::kClearAccessibilityFocus:
    case ax::mojom::Action::kCollapse:
    case ax::mojom::Action::kDecrement:
    case ax::mojom::Action::kDoDefault:
    case ax::mojom::Action::kExpand:
    case ax::mojom::Action::kIncrement:
    case ax::mojom::Action::kScrollToPoint:
    case ax::mojom::Action::kScrollToPositionAtRowColumn:
    case ax::mojom::Action::kFocus:
    case ax::mojom::Action::kSetAccessibilityFocus:
    case ax::mojom::Action::kSetScrollOffset:
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
    case ax::mojom::Action::kSetValue:
    case ax::mojom::Action::kShowContextMenu:
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollUp:
    case ax::mojom::Action::kScrollDown:
    case ax::mojom::Action::kScrollLeft:
    case ax::mojom::Action::kScrollRight:
    case ax::mojom::Action::kStitchChildTree:
      target->PerformAction(data);
      break;
    case ax::mojom::Action::kCustomAction:
    case ax::mojom::Action::kHitTest:
    case ax::mojom::Action::kReplaceSelectedText:
    case ax::mojom::Action::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
    case ax::mojom::Action::kGetTextLocation:
      break;
    case ax::mojom::Action::kAnnotatePageImages:
      // Handle this in AXAnnotatorsManager.
      break;
    case ax::mojom::Action::kSignalEndOfTest:
      HandleAXEvent(
          ui::AXEvent(ComputeRoot().AxID(), ax::mojom::Event::kEndOfTest));
      break;
    case ax::mojom::Action::kShowTooltip:
    case ax::mojom::Action::kHideTooltip:
    case ax::mojom::Action::kInternalInvalidateTree:
    case ax::mojom::Action::kResumeMedia:
    case ax::mojom::Action::kStartDuckingMedia:
    case ax::mojom::Action::kStopDuckingMedia:
    case ax::mojom::Action::kSuspendMedia:
    case ax::mojom::Action::kLongClick:
      break;
  }

  // Ensure the next serialization comes immediately after the action is
  // complete, even if the document is still loading.
  ScheduleImmediateAXUpdate();
}

void RenderAccessibilityImpl::Reset(uint32_t reset_token) {
  DCHECK(ax_context_);
  DCHECK(!accessibility_mode_.is_mode_off());
  ax_context_->ResetSerializer();
  set_reset_token(reset_token);
  FireLoadCompleteIfLoaded();
}

void RenderAccessibilityImpl::MarkWebAXObjectDirty(
    const WebAXObject& obj,
    ax::mojom::EventFrom event_from,
    ax::mojom::Action event_from_action,
    std::vector<ui::AXEventIntent> event_intents,
    ax::mojom::Event event_type) {
  DCHECK(obj.IsIncludedInTree())
      << "Cannot serialize unincluded object: " << obj.ToString().Utf8();

  obj.AddDirtyObjectToSerializationQueue(event_from, event_from_action,
                                         event_intents);
}

void RenderAccessibilityImpl::HandleAXEvent(const ui::AXEvent& event) {
  DCHECK(ax_context_);

  // All events sent to AXObjectCache from RAI need immediate serialization.
  ax_context_->AddEventToSerializationQueue(event, /* immediate */ true);
}

// TODO(accessibility): When legacy mode is deleted, calls to this function may
// be replaced with ax_context_->ScheduleImmediateSerialization()
void RenderAccessibilityImpl::ScheduleImmediateAXUpdate() {
  DCHECK(ax_context_);
  ax_context_->ScheduleImmediateSerialization();
}

bool RenderAccessibilityImpl::HasActiveDocument() const {
  DCHECK(ax_context_);
  return ax_context_->HasActiveDocument();
}

ui::AXMode RenderAccessibilityImpl::GetAXMode() const {
  return accessibility_mode_;
}

WebDocument RenderAccessibilityImpl::GetMainDocument() const {
  if (render_frame_ && render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->GetDocument();
  return WebDocument();
}

std::string RenderAccessibilityImpl::GetLanguage() {
  return page_language_;
}

bool RenderAccessibilityImpl::SendAccessibilitySerialization(
    std::vector<ui::AXTreeUpdate> updates,
    std::vector<ui::AXEvent> events,
    ui::AXLocationAndScrollUpdates location_and_scroll_updates,
    bool had_load_complete_messages) {
  if (had_load_complete_messages) {
    loading_stage_ = LoadingStage::kLoadCompleted;
  }

  TRACE_EVENT0(
      "accessibility",
      loading_stage_ == LoadingStage::kPostLoad
          ? "RenderAccessibilityImpl::SendPendingAccessibilityEvents"
          : "RenderAccessibilityImpl::SendPendingAccessibilityEventsLoading");
  base::ElapsedTimer timer;

  DCHECK(!accessibility_mode_.is_mode_off());
  DCHECK(ax_context_);
  DCHECK(ax_context_->IsSerializationInFlight());
  DCHECK(ax_context_->HasActiveDocument());

  CHECK(render_frame_);
  CHECK(render_frame_->in_frame_tree());

  // Don't send accessibility events for frames that don't yet have an tree id
  // as doing so will cause the browser to discard that message and all
  // subsequent ones.
  // TODO(crbug.com/40190596): There are some cases where no content is
  // currently rendered, due to an iframe returning 204 or window.stop() being
  // called. In these cases there will never be an AXTreeID as there is no
  // commit, which will prevent accessibility updates from ever being sent even
  // if the rendering is fixed. See also other TODOs related to 1231184 in this
  // file.
  DCHECK(render_frame_->GetWebFrame()->GetAXTreeID().token());

  WebDocument document = GetMainDocument();
  CHECK(!document.IsNull());

  // Don't serialize child trees without an embedding token. These are
  // unrendered child frames. This prevents a situation where child trees can't
  // be linked to their parent, leading to a dangerous situation for some
  // platforms, where events are fired on objects not connected to the root. For
  // example, on Mac, this can lead to a lockup in AppKit.
  DCHECK(document.GetFrame()->GetEmbeddingToken());

  WebAXObject root = ComputeRoot();
#if DCHECK_IS_ON()
  // Never causes a document lifecycle change during serialization,
  // because the assumption is that layout is in a safe, stable state.
  // (Skip if image_annotation_debugging_ is enabled because it adds
  // style attributes to images, affecting the document lifecycle
  // during accessibility.)
  std::unique_ptr<blink::WebDisallowTransitionScope> disallow;
  if (!image_annotation_debugging_) {
    disallow = std::make_unique<blink::WebDisallowTransitionScope>(&document);
  }
#endif

  // Save the page language.
  page_language_ = root.Language().Utf8();

#if DCHECK_IS_ON()
  // Protect against lifecycle changes in the popup document, if any.
  WebDocument popup_document = GetPopupDocument();
  std::optional<blink::WebDisallowTransitionScope> disallow2;
  if (!popup_document.IsNull()) {
    disallow2.emplace(&popup_document);
  }
#endif

  ui::AXUpdatesAndEvents updates_and_events;
  updates_and_events.updates = std::move(updates);
  updates_and_events.events = std::move(events);

  for (auto& update : updates_and_events.updates) {
    ax_annotators_manager_->Annotate(document, &update,
                                     had_load_complete_messages);
  }

  ax_annotators_manager_->AddDebuggingAttributes(updates_and_events.updates);

  CHECK(!weak_factory_for_pending_events_.HasWeakPtrs());
  CHECK(reset_token_);
  render_accessibility_manager_->HandleAccessibilityEvents(
      updates_and_events, location_and_scroll_updates, *reset_token_,
      base::BindOnce(&RenderAccessibilityImpl::OnSerializationReceived,
                     weak_factory_for_pending_events_.GetWeakPtr()));

  // Measure the amount of time spent in this function. Keep track of the
  // maximum within a time interval so we can upload UKM.
  base::TimeDelta elapsed_time_ms = timer.Elapsed();
  if (elapsed_time_ms > slowest_serialization_time_) {
    last_ukm_source_id_ = document.GetUkmSourceId();
    slowest_serialization_time_ = elapsed_time_ms;
  }
  // Also log the time taken in this function to track serialization
  // performance.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Accessibility.Performance.SendPendingAccessibilityEvents2",
      elapsed_time_ms, base::Microseconds(1), base::Seconds(1), 50);

  if (loading_stage_ == LoadingStage::kPostLoad) {
    // Track serialization after document load in order to measure the
    // contribution of serialization to interaction latency.
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Accessibility.Performance.SendPendingAccessibilityEvents.PostLoad2",
        elapsed_time_ms, base::Microseconds(1), base::Seconds(1), 50);
  }

  if (loading_stage_ == LoadingStage::kLoadCompleted) {
    loading_stage_ = LoadingStage::kPostLoad;
  }

  if (ukm_timer_->Elapsed() >= kMinUKMDelay) {
    MaybeSendUKM();
  }

  return true;
}

void RenderAccessibilityImpl::OnSerializationReceived() {
  DCHECK(ax_context_);
  ax_context_->OnSerializationReceived();
}

void RenderAccessibilityImpl::OnLoadInlineTextBoxes(
    const ui::AXActionTarget* target) {
  const BlinkAXActionTarget* blink_target =
      BlinkAXActionTarget::FromAXActionTarget(target);
  if (!blink_target) {
    return;
  }
  const WebAXObject& obj = blink_target->WebAXObject();

  DCHECK(ax_context_);
  obj.OnLoadInlineTextBoxes();

  // Explicitly send a tree change update event now.
  HandleAXEvent(ui::AXEvent(obj.AxID(), ax::mojom::Event::kTreeChanged));
}

void RenderAccessibilityImpl::OnGetImageData(const ui::AXActionTarget* target,
                                             const gfx::Size& max_size) {
  const BlinkAXActionTarget* blink_target =
      BlinkAXActionTarget::FromAXActionTarget(target);
  if (!blink_target) {
    return;
  }
  const WebAXObject& obj = blink_target->WebAXObject();
  obj.SetImageAsDataNodeId(max_size);

  const WebDocument& document = GetMainDocument();
  if (document.IsNull()) {
    return;
  }

  MarkWebAXObjectDirty(obj);
  HandleAXEvent(ui::AXEvent(obj.AxID(), ax::mojom::Event::kImageFrameUpdated,
                            ax::mojom::EventFrom::kAction,
                            ax::mojom::Action::kGetImageData));
}

void RenderAccessibilityImpl::OnDestruct() {
  render_frame_ = nullptr;
  delete this;
}

blink::WebDocument RenderAccessibilityImpl::GetPopupDocument() {
  blink::WebPagePopup* popup = render_frame_->GetWebView()->GetPagePopup();
  if (popup)
    return popup->GetDocument();
  return WebDocument();
}

WebAXObject RenderAccessibilityImpl::ComputeRoot() {
  DCHECK(render_frame_);
  DCHECK(render_frame_->GetWebFrame());
  return WebAXObject::FromWebDocument(GetMainDocument());
}

void RenderAccessibilityImpl::ConnectionClosed() {
  // This can happen when a navigation occurs with a serialization is in flight.
  // There is nothing special to do here.
  ax_context_->OnSerializationCancelled();
}

void RenderAccessibilityImpl::RecordInaccessiblePdfUkm() {
  ukm::builders::Accessibility_InaccessiblePDFs(
      GetMainDocument().GetUkmSourceId())
      .SetSeen(true)
      .Record(ukm_recorder_.get());
}

void RenderAccessibilityImpl::SetPluginAXTreeActionTargetAdapter(
    PluginAXTreeActionTargetAdapter* adapter) {
  plugin_action_target_adapter_ = adapter;
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

}  // namespace content
