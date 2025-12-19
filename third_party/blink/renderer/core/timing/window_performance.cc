/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/timing/window_performance.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "cc/base/features.h"
#include "components/viz/common/frame_timing_details.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/mojom/load_timing_info.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/input_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_hidden_state.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/interaction_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/timing/performance_container_timing.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_long_animation_frame_timing.h"
#include "third_party/blink/renderer/core/timing/performance_navigation_timing.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/performance_paint_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_entry.h"
#include "third_party/blink/renderer/core/timing/visibility_state_entry.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

static constexpr base::TimeDelta kLongTaskObserverThreshold =
    base::Milliseconds(50);

namespace blink {

namespace {

AtomicString GetFrameAttribute(HTMLFrameOwnerElement* frame_owner,
                               const QualifiedName& attr_name) {
  AtomicString attr_value;
  if (frame_owner->hasAttribute(attr_name)) {
    attr_value = frame_owner->getAttribute(attr_name);
  }
  return attr_value;
}

AtomicString GetFrameOwnerType(HTMLFrameOwnerElement* frame_owner) {
  switch (frame_owner->OwnerType()) {
    case FrameOwnerElementType::kNone:
      return performance_entry_names::kWindow;
    case FrameOwnerElementType::kIframe:
      return html_names::kIFrameTag.LocalName();
    case FrameOwnerElementType::kObject:
      return html_names::kObjectTag.LocalName();
    case FrameOwnerElementType::kEmbed:
      return html_names::kEmbedTag.LocalName();
    case FrameOwnerElementType::kFrame:
      return html_names::kFrameTag.LocalName();
    case FrameOwnerElementType::kFencedframe:
      return html_names::kFencedframeTag.LocalName();
  }
  NOTREACHED();
}

AtomicString GetFrameSrc(HTMLFrameOwnerElement* frame_owner) {
  switch (frame_owner->OwnerType()) {
    case FrameOwnerElementType::kObject:
      return GetFrameAttribute(frame_owner, html_names::kDataAttr);
    default:
      return GetFrameAttribute(frame_owner, html_names::kSrcAttr);
  }
}

const AtomicString& SelfKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, kSelfAttribution, ("self"));
  return kSelfAttribution;
}

const AtomicString& SameOriginAncestorKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, kSameOriginAncestorAttribution,
                      ("same-origin-ancestor"));
  return kSameOriginAncestorAttribution;
}

const AtomicString& SameOriginDescendantKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, kSameOriginDescendantAttribution,
                      ("same-origin-descendant"));
  return kSameOriginDescendantAttribution;
}

const AtomicString& SameOriginKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, kSameOriginAttribution,
                      ("same-origin"));
  return kSameOriginAttribution;
}

AtomicString SameOriginAttribution(Frame* observer_frame,
                                   Frame* culprit_frame) {
  DCHECK(IsMainThread());
  if (observer_frame == culprit_frame) {
    return SelfKeyword();
  }
  if (observer_frame->Tree().IsDescendantOf(culprit_frame)) {
    return SameOriginAncestorKeyword();
  }
  if (culprit_frame->Tree().IsDescendantOf(observer_frame)) {
    return SameOriginDescendantKeyword();
  }
  return SameOriginKeyword();
}

// Eligible event types should be kept in sync with
// WebInputEvent::IsWebInteractionEvent().
bool IsEventTypeForInteractionId(const AtomicString& type) {
  return type == event_type_names::kPointercancel ||
         type == event_type_names::kContextmenu ||
         type == event_type_names::kPointerdown ||
         type == event_type_names::kPointerup ||
         type == event_type_names::kClick ||
         type == event_type_names::kKeydown ||
         type == event_type_names::kKeypress ||
         type == event_type_names::kKeyup ||
         type == event_type_names::kCompositionstart ||
         type == event_type_names::kCompositionupdate ||
         type == event_type_names::kCompositionend ||
         type == event_type_names::kInput;
}

base::TimeDelta TotalNonOverlappingProcessingDuration(
    HeapVector<Member<PerformanceEventTiming>> event_timing_entries) {
  base::TimeDelta processing_duration;
  for (const auto& entry : event_timing_entries) {
    const auto& processing_start_time =
        entry->GetEventTimingReportingInfo()->processing_start_time;
    const auto& processing_end_time =
        entry->GetEventTimingReportingInfo()->processing_end_time;
    if (!entry->GetEventTimingReportingInfo()
             ->is_processing_fully_nested_in_another_event) {
      processing_duration += processing_end_time - processing_start_time;
    }
  }
  return processing_duration;
}

}  // namespace

constexpr size_t kDefaultVisibilityStateEntrySize = 50;

const char kHistogramEventCreationTimeToProcessingStartPerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.EventCreationTimeToProcessingStart";
const char kHistogramEventQueueTimeToProcessingStartPerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.EventQueueTimeToProcessingStart";
const char kHistogramEventCreationTimeToEventQueueTimePerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.EventCreationTimeToEventQueueTime";

const char
    kHistogramFirstProcessingStartToLastProcessingEndPerAnimationFrame[] =
        "Blink.Responsiveness.PerAnimationFrame."
        "FirstProcessingStartToLastProcessingEnd";
const char kHistogramTotalUnaccountedEventProcessingTimePerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame."
    "TotalUnaccountedEventProcessingTime";

const char kHistogramProcessingEndToRenderStartTimePerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.ProcessingEndToRenderStartTime";
const char kHistogramRenderStartTimeToCommitTimePerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.RenderStartTimeToCommitTime";
const char kHistogramCommitToPresentationTimePerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.CommitToPresentationTime";
const char kHistogramProcessingEndToPresentationTimePerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.ProcessingEndToPresentationTime";

const char kHistogramEventQueueTimeToCommitPerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.EventQueueTimeToCommit";
const char kHistogramEventCreationToPresentationTimePerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.EventCreationToPresentationTime";
const char kHistogramEventCreationToLastProcessingEndPerAnimationFrame[] =
    "Blink.Responsiveness.PerAnimationFrame.EventCreationToLastProcessingEnd."
    "NoFramePresented";

const char kHistogramBothInteractionTypes[] = ".Both";
const char kHistogramKeyboardInteractionTypes[] = ".Keyboard";
const char kHistogramTapOrClickInteractionTypes[] = ".TapOrClick";
const char kHistogramAllInteractionTypes[] = ".All";

base::TimeTicks WindowPerformance::GetTimeOrigin(LocalDOMWindow* window) {
  DocumentLoader* loader = window->GetFrame()->Loader().GetDocumentLoader();
  return loader->GetTiming().ReferenceMonotonicTime();
}

WindowPerformance::WindowPerformance(LocalDOMWindow* window)
    : Performance(GetTimeOrigin(window),
                  window->CrossOriginIsolatedCapability(),
                  window->GetTaskRunner(TaskType::kPerformanceTimeline),
                  window),
      ExecutionContextClient(window),
      PageVisibilityObserver(window->GetFrame()->GetPage()),
      responsiveness_metrics_(
          MakeGarbageCollected<ResponsivenessMetrics>(this)) {
  DCHECK(window);
  DCHECK(window->GetFrame()->GetPerformanceMonitor());
  if (!RuntimeEnabledFeatures::LongTaskFromLongAnimationFrameEnabled()) {
    window->GetFrame()->GetPerformanceMonitor()->Subscribe(
        PerformanceMonitor::kLongTask, kLongTaskObserverThreshold, this);
  }

  DCHECK(GetPage());
  AddVisibilityStateEntry(GetPage()->IsPageVisible(), base::TimeTicks());
}

WindowPerformance::~WindowPerformance() = default;

ExecutionContext* WindowPerformance::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

PerformanceTiming* WindowPerformance::timing() const {
  if (!timing_) {
    timing_ = MakeGarbageCollected<PerformanceTiming>(DomWindow());
  }

  return timing_.Get();
}

PerformanceTimingForReporting* WindowPerformance::timingForReporting() const {
  if (!timing_for_reporting_) {
    timing_for_reporting_ =
        MakeGarbageCollected<PerformanceTimingForReporting>(DomWindow());
  }

  return timing_for_reporting_.Get();
}

PerformanceNavigation* WindowPerformance::navigation() const {
  if (!navigation_) {
    navigation_ = MakeGarbageCollected<PerformanceNavigation>(DomWindow());
  }

  return navigation_.Get();
}

MemoryInfo* WindowPerformance::memory(ScriptState* script_state) const {
  // The performance.memory() API has been improved so that we report precise
  // values when the process is locked to a site. The intent (which changed
  // course over time about what changes would be implemented) can be found at
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/no00RdMnGio,
  // and the relevant bug is https://crbug.com/807651.
  const bool is_locked_to_site = Platform::Current()->IsLockedToSite();
  auto* memory_info = MakeGarbageCollected<MemoryInfo>(
      is_locked_to_site ? MemoryInfo::Precision::kPrecise
                        : MemoryInfo::Precision::kBucketized);
  // Record Web Memory UKM.
  const uint64_t kBytesInKB = 1024;
  auto* execution_context = ExecutionContext::From(script_state);
  ukm::builders::PerformanceAPI_Memory_Legacy(execution_context->UkmSourceID())
      .SetJavaScript(memory_info->usedJSHeapSize() / kBytesInKB)
      .Record(execution_context->UkmRecorder());
  if (!is_locked_to_site) {
    UseCounter::Count(
        execution_context,
        WebFeature::kV8Performance_Memory_AttributeGetter_NotLockedToSite);
  }
  return memory_info;
}

namespace {

BASE_FEATURE(kAdjustNavigationalPrefetchTiming,
             base::FEATURE_ENABLED_BY_DEFAULT);

enum class AdjustNavigationalPrefetchTimingBehavior {
  kRemoveLoadTiming,
  kClampToFetchStart,
};

constexpr base::FeatureParam<AdjustNavigationalPrefetchTimingBehavior>::Option
    kAdjustNavigationalPrefetchTimingBehaviorOptions[] = {
        {AdjustNavigationalPrefetchTimingBehavior::kRemoveLoadTiming,
         "remove_load_timing"},
        {AdjustNavigationalPrefetchTimingBehavior::kClampToFetchStart,
         "clamp_to_fetch_start"},
};

constexpr base::FeatureParam<AdjustNavigationalPrefetchTimingBehavior>
    kAdjustNavigationalPrefetchTimingBehavior{
        &kAdjustNavigationalPrefetchTiming,
        "adjust_navigational_prefetch_timing_behavior",
        AdjustNavigationalPrefetchTimingBehavior::kClampToFetchStart,
        &kAdjustNavigationalPrefetchTimingBehaviorOptions};

network::mojom::blink::LoadTimingInfoPtr
AdjustLoadTimingForNavigationalPrefetch(
    const DocumentLoadTiming& document_load_timing,
    network::mojom::blink::LoadTimingInfoPtr timing) {
  if (!base::FeatureList::IsEnabled(kAdjustNavigationalPrefetchTiming)) {
    return timing;
  }

  static const auto behavior = kAdjustNavigationalPrefetchTimingBehavior.Get();
  switch (behavior) {
    case AdjustNavigationalPrefetchTimingBehavior::kRemoveLoadTiming:
      return nullptr;

    case AdjustNavigationalPrefetchTimingBehavior::kClampToFetchStart:
      break;
  }

  // Everything that happened before the fetch start (this is the value that
  // will be exposed as fetchStart on PerformanceNavigationTiming).
  using network::mojom::blink::LoadTimingInfo;
  using network::mojom::blink::LoadTimingInfoConnectTiming;
  const base::TimeTicks min_ticks = document_load_timing.FetchStart();
  auto new_timing = LoadTimingInfo::New();
  new_timing->socket_reused = timing->socket_reused;
  new_timing->socket_log_id = timing->socket_log_id;

  // Copy the basic members of LoadTimingInfo, and clamp them.
  for (base::TimeTicks LoadTimingInfo::*ts :
       {&LoadTimingInfo::request_start, &LoadTimingInfo::send_start,
        &LoadTimingInfo::send_end, &LoadTimingInfo::receive_headers_start,
        &LoadTimingInfo::receive_headers_end,
        &LoadTimingInfo::receive_non_informational_headers_start,
        &LoadTimingInfo::first_early_hints_time}) {
    if (!((*timing).*ts).is_null()) {
      (*new_timing).*ts = std::max((*timing).*ts, min_ticks);
    }
  }

  // If connect timing is available, do the same to it.
  if (auto* connect_timing = timing->connect_timing.get()) {
    new_timing->connect_timing = LoadTimingInfoConnectTiming::New();
    auto& new_connect_timing = *new_timing->connect_timing;
    for (base::TimeTicks LoadTimingInfoConnectTiming::*ts : {
             &LoadTimingInfoConnectTiming::domain_lookup_start,
             &LoadTimingInfoConnectTiming::domain_lookup_end,
             &LoadTimingInfoConnectTiming::connect_start,
             &LoadTimingInfoConnectTiming::connect_end,
             &LoadTimingInfoConnectTiming::ssl_start,
             &LoadTimingInfoConnectTiming::ssl_end,
         }) {
      if (!(connect_timing->*ts).is_null()) {
        new_connect_timing.*ts = std::max(connect_timing->*ts, min_ticks);
      }
    }
  }

  return new_timing;
}

}  // namespace

void WindowPerformance::CreateNavigationTimingInstance(
    mojom::blink::ResourceTimingInfoPtr info) {
  DCHECK(DomWindow());

  // If this is navigational prefetch, it may be necessary to partially redact
  // the timings to avoid exposing when events that occurred during the prefetch
  // happened. Instead, they look like they happened very fast.
  DocumentLoader* loader = DomWindow()->document()->Loader();
  if (loader &&
      loader->GetNavigationDeliveryType() ==
          network::mojom::NavigationDeliveryType::kNavigationalPrefetch &&
      info->timing) {
    info->timing = AdjustLoadTimingForNavigationalPrefetch(
        loader->GetTiming(), std::move(info->timing));
  }

  navigation_timing_ = MakeGarbageCollected<PerformanceNavigationTiming>(
      *DomWindow(), std::move(info), time_origin_, NavigationId());
}

void WindowPerformance::OnBodyLoadFinished(int64_t encoded_body_size,
                                           int64_t decoded_body_size) {
  if (navigation_timing_) {
    navigation_timing_->OnBodyLoadFinished(encoded_body_size,
                                           decoded_body_size);
  }
}

void WindowPerformance::BuildJSONValue(V8ObjectBuilder& builder) const {
  Performance::BuildJSONValue(builder);
  builder.Add("timing", timing());
  builder.Add("navigation", navigation());
}

void WindowPerformance::Trace(Visitor* visitor) const {
  visitor->Trace(event_timing_entries_);
  visitor->Trace(first_pointer_down_event_timing_);
  visitor->Trace(event_counts_);
  visitor->Trace(navigation_);
  visitor->Trace(timing_);
  visitor->Trace(timing_for_reporting_);
  visitor->Trace(responsiveness_metrics_);
  visitor->Trace(current_event_);
  Performance::Trace(visitor);
  PerformanceMonitor::Client::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

static bool CanAccessOrigin(Frame* frame1, Frame* frame2) {
  const SecurityOrigin* security_origin1 =
      frame1->GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* security_origin2 =
      frame2->GetSecurityContext()->GetSecurityOrigin();
  return security_origin1->CanAccess(security_origin2);
}

/**
 * Report sanitized name based on cross-origin policy.
 * See detailed Security doc here: http://bit.ly/2duD3F7
 */
// static
std::pair<AtomicString, DOMWindow*> WindowPerformance::SanitizedAttribution(
    ExecutionContext* task_context,
    bool has_multiple_contexts,
    LocalFrame* observer_frame) {
  DCHECK(IsMainThread());
  if (has_multiple_contexts) {
    // Unable to attribute, multiple script execution contents were involved.
    DEFINE_STATIC_LOCAL(const AtomicString, kAmbiguousAttribution,
                        ("multiple-contexts"));
    return std::make_pair(kAmbiguousAttribution, nullptr);
  }

  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(task_context);
  if (!window || !window->GetFrame()) {
    // Unable to attribute as no script was involved.
    DEFINE_STATIC_LOCAL(const AtomicString, kUnknownAttribution, ("unknown"));
    return std::make_pair(kUnknownAttribution, nullptr);
  }

  // Exactly one culprit location, attribute based on origin boundary.
  Frame* culprit_frame = window->GetFrame();
  DCHECK(culprit_frame);
  if (CanAccessOrigin(observer_frame, culprit_frame)) {
    // From accessible frames or same origin, return culprit location URL.
    return std::make_pair(SameOriginAttribution(observer_frame, culprit_frame),
                          culprit_frame->DomWindow());
  }
  // For cross-origin, if the culprit is the descendant or ancestor of
  // observer then indicate the *closest* cross-origin frame between
  // the observer and the culprit, in the corresponding direction.
  if (culprit_frame->Tree().IsDescendantOf(observer_frame)) {
    // If the culprit is a descendant of the observer, then walk up the tree
    // from culprit to observer, and report the *last* cross-origin (from
    // observer) frame.  If no intermediate cross-origin frame is found, then
    // report the culprit directly.
    Frame* last_cross_origin_frame = culprit_frame;
    for (Frame* frame = culprit_frame; frame != observer_frame;
         frame = frame->Tree().Parent()) {
      if (!CanAccessOrigin(observer_frame, frame)) {
        last_cross_origin_frame = frame;
      }
    }
    DEFINE_STATIC_LOCAL(const AtomicString, kCrossOriginDescendantAttribution,
                        ("cross-origin-descendant"));
    return std::make_pair(kCrossOriginDescendantAttribution,
                          last_cross_origin_frame->DomWindow());
  }
  if (observer_frame->Tree().IsDescendantOf(culprit_frame)) {
    DEFINE_STATIC_LOCAL(const AtomicString, kCrossOriginAncestorAttribution,
                        ("cross-origin-ancestor"));
    return std::make_pair(kCrossOriginAncestorAttribution, nullptr);
  }
  DEFINE_STATIC_LOCAL(const AtomicString, kCrossOriginAttribution,
                      ("cross-origin-unreachable"));
  return std::make_pair(kCrossOriginAttribution, nullptr);
}

void WindowPerformance::ReportLongTask(base::TimeTicks start_time,
                                       base::TimeTicks end_time,
                                       ExecutionContext* task_context,
                                       bool has_multiple_contexts) {
  if (!DomWindow()) {
    return;
  }
  std::pair<AtomicString, DOMWindow*> attribution =
      WindowPerformance::SanitizedAttribution(
          task_context, has_multiple_contexts, DomWindow()->GetFrame());
  DOMWindow* culprit_dom_window = attribution.second;
  if (!culprit_dom_window || !culprit_dom_window->GetFrame() ||
      !culprit_dom_window->GetFrame()->DeprecatedLocalOwner()) {
    AddLongTaskTiming(start_time, end_time, attribution.first,
                      performance_entry_names::kWindow, g_empty_atom,
                      g_empty_atom, g_empty_atom);
  } else {
    HTMLFrameOwnerElement* frame_owner =
        culprit_dom_window->GetFrame()->DeprecatedLocalOwner();
    AddLongTaskTiming(start_time, end_time, attribution.first,
                      GetFrameOwnerType(frame_owner), GetFrameSrc(frame_owner),
                      GetFrameAttribute(frame_owner, html_names::kIdAttr),
                      GetFrameAttribute(frame_owner, html_names::kNameAttr));
  }
}

void WindowPerformance::EventTimingProcessingStart(
    const Event& event,
    base::TimeTicks processing_start,
    EventTarget* hit_test_target) {
  if (!DomWindow() || !DomWindow()->GetFrame()) {
    return;
  }
  DCHECK(!processing_start.is_null());

  const AtomicString& event_type = event.type();

  // Event Counts API.
  eventCounts()->Add(event_type);

  // Some events are neither pointer nor keyboard (i.e. mouse events)
  // But we only use pointer and keyboard event data for interactions.
  const PointerEvent* pointer_event = DynamicTo<PointerEvent>(event);
  const KeyboardEvent* key_event = DynamicTo<KeyboardEvent>(event);

  PerformanceEventTiming::EventTimingReportingInfo reporting_info{
      .enqueued_to_main_thread_time =
          responsiveness_metrics_->CurrentInteractionEventQueuedTimestamp(),
      .processing_start_time = processing_start,
  };

  if (pointer_event) {
    reporting_info.creation_time = pointer_event->OldestPlatformTimeStamp();
    reporting_info.pointer_id = pointer_event->pointerId();

    if (event_type == "pointerdown") {
      pending_pointer_down_start_time_ = reporting_info.creation_time;
    }

    reporting_info.prevent_counting_as_interaction =
        pointer_event->GetPreventCountingAsInteraction();
  } else {
    reporting_info.creation_time = event.PlatformTimeStamp();

    if (key_event) {
      reporting_info.key_code = key_event->keyCode();
    }
  }

  // Set prevent_counting_as_interaction to true for all the event entries when
  // the selection autoscroll happens at the current event presentation frame
  // or the previous frame.
  reporting_info.prevent_counting_as_interaction |= IsAutoscrollActive();

  // We always have a Hit test target before starting event dispatch.  During
  // event dispatch we might change target via event retargetting or
  // pointer-capture (or any number of other features).
  // The "final" target is attached to the blink::Event as target().  However,
  // its possible that we optimize out the event dispatch steps (i.e. we don't
  // have listeners).  When that happens, Event Timing still measures and
  // reports entries, but Chromium leaves the blink::Event target() value as
  // nullptr.  So, we cannot rely on always having a target().  We use the
  // following strategy:
  // 1. Start with `hit_test_target`, from ProcessingStart, before dispatch.
  // 2. Update to `event.target()`, from ProcessingEnd, if we can.
  // `hit_test_target` can still be null in tests.
  // `target` can be non-null but detached from DOM and GC-ed before observer
  // fires.
  PerformanceEventTiming* entry = PerformanceEventTiming::Create(
      event_type, reporting_info, event.cancelable(), hit_test_target,
      DomWindow(), NavigationId());

  event_timing_entries_.push_back(entry);
  current_event_ = &event;
}

void WindowPerformance::EventTimingProcessingEnd(
    const Event& event,
    base::TimeTicks processing_end) {
  current_event_ = nullptr;
  DCHECK(!processing_end.is_null());

  if (!DomWindow() || !DomWindow()->GetFrame()) {
    return;
  }
  const AtomicString& event_type = event.type();
  auto iter = std::find_if(event_timing_entries_.rbegin(),
                           event_timing_entries_.rend(), [](const auto& event) {
                             return event->GetEventTimingReportingInfo()
                                 ->processing_end_time.is_null();
                           });
  CHECK(iter != event_timing_entries_.rend());
  PerformanceEventTiming* entry = *iter;
  CHECK(entry);
  CHECK(entry->name() == event_type);

  PerformanceEventTiming::EventTimingReportingInfo* reporting_info =
      entry->GetEventTimingReportingInfo();
  CHECK(reporting_info);
  reporting_info->processing_end_time = processing_end;

  if (auto pre_iter = std::next(iter);
      pre_iter != event_timing_entries_.rend()) {
    auto iter_null_time = std::find_if(
        event_timing_entries_.begin(), pre_iter.base(), [](const auto& event) {
          return event->GetEventTimingReportingInfo()
              ->processing_end_time.is_null();
        });
    if (iter_null_time != pre_iter.base()) {
      reporting_info->is_processing_fully_nested_in_another_event = true;
    }
  }

  // "Artificial" pointerup events will re-use the same timestamp as the
  // pointerdown, leading to large delays. crbug.com/1321819.
  if ((event_type == event_type_names::kPointerup ||
       event_type == event_type_names::kClick) &&
      reporting_info->creation_time == pending_pointer_down_start_time_) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingArtificialPointerupOrClick);
#if BUILDFLAG(IS_MAC)
    // MacOS in particular seems to have an issue measuring these, because
    // it leads to arbitrarily long presentation delays. crbug.com/1321819.
    entry->UpdateFallbackTime(processing_end,
                              FallbackReason::kMacOSArtificialEvent);
#endif  // BUILDFLAG(IS_MAC)
  }

  if (event.RawTarget()) {
    // `event->target()` is assigned as part of EventDispatch, and will be unset
    // whenever we skip dispatch. (See: crbug.com/1367329).
    // Note: target may be dom detached, and even GC-ed, before Observer fires.
    entry->SetTarget(event.RawTarget());
  }

  // Request presentation time first, because this might increment presentation
  // index
  // TODO(crbug.com/)
  if (need_new_promise_for_event_presentation_time_) {
    DomWindow()->GetFrame()->GetChromeClient().NotifyPresentationTime(
        *DomWindow()->GetFrame(),
        BindOnce(&WindowPerformance::OnPresentationPromiseResolved,
                 WrapWeakPersistent(this), ++event_presentation_promise_count_,
                 // TODO(crbug.com/378647854): Current implementation uses
                 // source id from previous BeginMainFrame as an
                 // approximate. And this can be further improved to the
                 // current BeginMainFrame if we could defer presentation
                 // promise registering to align with each BeginMainFrame.
                 begin_main_frame_source_id_));
    need_new_promise_for_event_presentation_time_ = false;
  }

  reporting_info->presentation_index = event_presentation_promise_count_;
}

void WindowPerformance::SetCommitFinishTimeStampForPendingEvents(
    base::TimeTicks commit_finish_time) {
  for (const auto& entry : event_timing_entries_) {
    // Skip events that already have a commit time
    if (!entry->GetEventTimingReportingInfo()->commit_finish_time.is_null()) {
      continue;
    }
    // Skip events that don't need a next paint measure.
    if (!entry->NeedsNextPaintMeasurement()) {
      continue;
    }
    // The following check should be true in typical conditions, but seems to
    // fail whenever we see multiple OnPaintFinished (with multiple
    // EventProcessingEnd) without a Commit after each Paint.
    // CHECK(entry->GetEventTimingReportingInfo()->presentation_index ==
    //       event_presentation_promise_count_);
    entry->GetEventTimingReportingInfo()->commit_finish_time =
        commit_finish_time;
  }
}

void WindowPerformance::SetRenderStartTimeForPendingEvents(
    base::TimeTicks render_start_time) {
  for (const auto& entry : event_timing_entries_) {
    // Skip events that already have a render start time.
    if (!entry->GetEventTimingReportingInfo()->render_start_time.is_null()) {
      continue;
    }
    // Skip events that don't need a next paint measure.
    if (!entry->NeedsNextPaintMeasurement()) {
      continue;
    }
    entry->GetEventTimingReportingInfo()->render_start_time = render_start_time;
  }
}

// Important details:
// 1. presentation_index and expected_frame_source_id are "captured" at the
// time the presentation is requested, and might have changed by the time
// presentation time arrives.
// 2. presentation time might be "fake" when broken swap promise.
void WindowPerformance::OnPresentationPromiseResolved(
    uint64_t presentation_index,
    uint64_t expected_frame_source_id,
    const viz::FrameTimingDetails& presentation_details) {
  if (!DomWindow() || !DomWindow()->document()) {
    return;
  }

  // If the resolved presentation promise is for an animation frame that didn't
  // observe OnPaintFinished, then the most recent events actually did not need
  // next paint.  We need to mark these with a fallback time instead of a real
  // presentation time.
  // TODO(crbug.com/378647854): Move this to happen before we request
  // presentation time, when we dont need next paint, rather than after.
  if (presentation_index == event_presentation_promise_count_ &&
      !need_new_promise_for_event_presentation_time_) {
    ReportEventTimingsWithoutNextPaint(
        presentation_details.presentation_feedback.timestamp);
    need_new_promise_for_event_presentation_time_ = true;
    return;
  }

  // We assume the presentation is for the expected source unless it's proven to
  // be wrong.
  uint64_t actual_frame_source_id = presentation_details.frame_id.source_id;
  bool is_presentation_for_expected_source =
      !expected_frame_source_id || !actual_frame_source_id ||
      expected_frame_source_id == actual_frame_source_id;
  if (base::FeatureList::IsEnabled(
          ::features::kInternalBeginFrameSourceOnManyDidNotProduceFrame) ||
      base::FeatureList::IsEnabled(::features::kManualBeginFrame)) {
    // Switch to cc BeginFrameSource will generate kNotRestartable(0) begin
    // frame and submit compositor frame with kManualSourceId.
    if ((expected_frame_source_id >> 32) == 0 ||
        actual_frame_source_id == viz::BeginFrameArgs::kManualSourceId) {
      is_presentation_for_expected_source = true;
    }
  }

  for (const auto& entry : event_timing_entries_) {
    auto* timing = entry->GetEventTimingReportingInfo();
    if (timing->presentation_index == presentation_index) {
      timing->presentation_time =
          presentation_details.presentation_feedback.timestamp;

      if (!is_presentation_for_expected_source) {
        if (base::FeatureList::IsEnabled(
                features::
                    kEventTimingIgnorePresentationTimeFromUnexpectedFrameSource)) {
          CHECK(!timing->commit_finish_time.is_null());
          entry->UpdateFallbackTime(timing->commit_finish_time,
                                    FallbackReason::kUnexpectedFrameSource);
        }
      }

      // If page visibility was changed, add a fallback_time to the entry's
      // processingEnd. Because we already flush events in
      // `ReportAllPendingEventTimingsOnPageHidden`, this should only happen if
      // a new event is processed after visibility is changed.  Users cannot
      // interact with a hidden page, but, there might have been events in queue
      // when the page was hidden (and they couldn't be flushed because they
      // weren't even dispatched yet).
      // TODO(crbug.com/378647854): We might want to just check for this at
      // event timing registration time.  If the page is currently hidden (or
      // was made hidden after the event was created/enqueued), then just skip
      // asking for presentation time.
      if (last_hidden_timestamp_ > timing->creation_time &&
          last_hidden_timestamp_ < timing->presentation_time) {
        if (!timing->commit_finish_time.is_null() &&
            last_hidden_timestamp_ > timing->commit_finish_time) {
          entry->UpdateFallbackTime(timing->commit_finish_time,
                                    FallbackReason::kVisibilityChange);
        } else {
          entry->UpdateFallbackTime(timing->processing_end_time,
                                    FallbackReason::kVisibilityChange);
        }
      }

      // A javascript synchronous modal dialog might show before the event
      // frame got presented.  If so, we use a fallback time to the dialog
      // showing time.
      // TODO(crbug.com/378647854): Simplify the way we measure dialogs:
      // - Replace the list of dialogs with a single timestamp
      // - When we see the first dialog per animation frame, resolve all
      //    events already in queue (similar to visibility change).
      // - When we process a new event, if we've already seen a modal, use it
      //    as a fallback time.
      // - We also don't need to fallback to dialog time after Paint is
      //    committed, since paint will show at that point.
      while (!show_modal_dialog_timestamps_.empty() &&
             show_modal_dialog_timestamps_.front() < timing->creation_time) {
        show_modal_dialog_timestamps_.pop_front();
      }
      if (!show_modal_dialog_timestamps_.empty() &&
          show_modal_dialog_timestamps_.front() < timing->presentation_time) {
        entry->UpdateFallbackTime(show_modal_dialog_timestamps_.front(),
                                  FallbackReason::kModalDialog);
      }
    }
  }
  ReportEventTimings();
}

void WindowPerformance::ReportEventTimingsWithoutNextPaint(
    base::TimeTicks fallback_time) {
  for (const auto& event_timing_entry : event_timing_entries_) {
    if (event_timing_entry->GetEventTimingReportingInfo()->presentation_index ==
        event_presentation_promise_count_) {
      event_timing_entry->UpdateFallbackTime(
          fallback_time, FallbackReason::kDoesNotNeedNextPaint);
    }
  }
  ReportEventTimings();
}

void WindowPerformance::FlushEventTimingsOnPageHidden() {
  ReportAllPendingEventTimingsOnPageHidden();

  // Remove any remaining events that are not flushed by the above step.
  responsiveness_metrics_->FlushAllEventsAtPageHidden();
}

void ReportPerAnimationFrameHistograms(std::string_view histogram_name,
                                       std::string_view histogram_suffix,
                                       base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(
      base::StrCat({histogram_name, histogram_suffix}), sample,
      base::Milliseconds(1), base::Seconds(60), 50);
  base::UmaHistogramCustomTimes(
      base::StrCat({histogram_name, kHistogramAllInteractionTypes}), sample,
      base::Milliseconds(1), base::Seconds(60), 50);
}

// At visibility change, we report event timings of current pending events. The
// registered presentation callback, when invoked, would be ignored.
void WindowPerformance::ReportAllPendingEventTimingsOnPageHidden() {
  // By the time visibility change happens, DomWindow object should still be
  // alive. This is just to be safe.
  if (!DomWindow() || !DomWindow()->document()) {
    return;
  }

  // For events which don't have an end_time yet, set a fallback time to the
  // processingEnd timestamp.
  // Note: some events won't have a processingEnd time yet.  This can happen
  // with nested event loops running in the middle of an event dispatch.
  // Skip assigning a fallback when that happens.
  // Ideally the fallback time could be the last_hidden_timestamp_, but we don't
  // actually have an accurate value for that (it would need to come from
  // browser IPC).
  for (const auto& event_timing_entry : event_timing_entries_) {
    auto* entryInfo = event_timing_entry->GetEventTimingReportingInfo();
    bool has_no_known_end_time = !event_timing_entry->HasKnownEndTime();
    bool has_processing_end_time = !entryInfo->processing_end_time.is_null();

    if (has_no_known_end_time && has_processing_end_time) {
      event_timing_entry->UpdateFallbackTime(entryInfo->processing_end_time,
                                             FallbackReason::kVisibilityChange);
    }
  }
  ReportEventTimings();
}

void WindowPerformance::ReportEventTimings() {
  CHECK(DomWindow() && DomWindow()->document());
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*(DomWindow()->document()));

  bool tracing_enabled = TRACE_EVENT_CATEGORY_ENABLED("latency");
  const auto parent_track =
      perfetto::NamedTrack::ThreadScoped("EventTimingsByAnimationFrame", this);

  while (!event_timing_entries_.empty()) {
    // Find the range [first, last) of events with the same presentation_index
    auto first = event_timing_entries_.begin();
    uint64_t presentation_index =
        first->Get()->GetEventTimingReportingInfo()->presentation_index;
    auto last = std::find_if_not(
        first, event_timing_entries_.end(), [presentation_index](auto entry) {
          return presentation_index ==
                 entry->GetEventTimingReportingInfo()->presentation_index;
        });

    // Unless ALL events in this range are ready to be reported, break out.
    // Today: only a known EndTime is needed.
    // Soon: also enforce interactionID to know Known.
    if (!std::all_of(first, last,
                     [](auto entry) { return entry->IsReadyForReporting(); })) {
      break;
    }

    auto* first_event_reporting_info =
        first->Get()->GetEventTimingReportingInfo();
    auto* last_event_reporting_info =
        std::prev(last)->Get()->GetEventTimingReportingInfo();
    const auto& first_event_creation_time =
        first_event_reporting_info->creation_time;
    const auto& first_event_enqueued_to_main_thread_time =
        first_event_reporting_info->enqueued_to_main_thread_time;
    const auto& first_event_processing_start =
        first_event_reporting_info->processing_start_time;
    const auto& last_event_processing_end_time =
        last_event_reporting_info->processing_end_time;
    const auto& last_event_render_start_time =
        last_event_reporting_info->render_start_time;
    const auto& last_event_commit_finish_time =
        last_event_reporting_info->commit_finish_time;
    const auto& last_event_presentation_time =
        last_event_reporting_info->presentation_time;
    const auto& frame_end_time = !last_event_commit_finish_time.is_null()
                                     ? last_event_commit_finish_time
                                     : last_event_reporting_info->fallback_time;

    if (tracing_enabled) {
      auto flowid = perfetto::Flow::ProcessScoped(presentation_index);

      TRACE_EVENT_BEGIN("latency", "EventsInAnimationFrame", parent_track,
                        first_event_processing_start, flowid);

      TRACE_EVENT_INSTANT("latency", "EventCreation", parent_track,
                          first_event_creation_time, flowid);
    }

    // Report all the events in this frame
    bool had_interaction_in_animation_frame = false;
    bool had_key_interaction = false;
    bool had_click_tap_interaction = false;
    std::for_each(first, last, [&](auto entry) {
      ReportEvent(interactive_detector, entry);
      if (entry->HasKnownInteractionID() && entry->interactionId() != 0u) {
        had_interaction_in_animation_frame = true;
        if (entry->GetEventTimingReportingInfo()->key_code.has_value()) {
          had_key_interaction = true;
        }
        if (entry->GetEventTimingReportingInfo()->pointer_id.has_value()) {
          had_click_tap_interaction = true;
        }
      }
    });

    if (tracing_enabled) {
      auto flowid = perfetto::Flow::ProcessScoped(presentation_index);

      TRACE_EVENT_END("latency", parent_track, frame_end_time);

      if (!last_event_presentation_time.is_null()) {
        TRACE_EVENT_INSTANT("latency", "EventPresentation", parent_track,
                            last_event_presentation_time, flowid);
      }

      if (auto first_entry_with_fallback =
              std::find_if(first, last,
                           [](auto entry) {
                             return !entry->GetEventTimingReportingInfo()
                                         ->fallback_time.is_null();
                           });
          first_entry_with_fallback != last) {
        TRACE_EVENT_INSTANT("latency", "EventFallbackTime", parent_track,
                            first_entry_with_fallback->Get()
                                ->GetEventTimingReportingInfo()
                                ->fallback_time,
                            flowid);
      }
    }

    // Report INP breakdown metrics into UMA per animation frame.
    // Input delay breakdown.
    if (had_interaction_in_animation_frame) {
      std::string_view histogram_suffix;
      if (had_click_tap_interaction && had_key_interaction) {
        histogram_suffix = kHistogramBothInteractionTypes;
      } else if (had_key_interaction) {
        histogram_suffix = kHistogramKeyboardInteractionTypes;
      } else {
        histogram_suffix = kHistogramTapOrClickInteractionTypes;
      }
      ReportPerAnimationFrameHistograms(
          kHistogramEventCreationTimeToProcessingStartPerAnimationFrame,
          histogram_suffix,
          first_event_processing_start - first_event_creation_time);
      ReportPerAnimationFrameHistograms(
          kHistogramEventCreationTimeToEventQueueTimePerAnimationFrame,
          histogram_suffix,
          first_event_enqueued_to_main_thread_time - first_event_creation_time);
      ReportPerAnimationFrameHistograms(
          kHistogramEventQueueTimeToProcessingStartPerAnimationFrame,
          histogram_suffix,
          first_event_processing_start -
              first_event_enqueued_to_main_thread_time);

      // Event Processing duration breakdown.
      base::TimeDelta total_processing_duration =
          last_event_processing_end_time - first_event_processing_start;
      ReportPerAnimationFrameHistograms(
          kHistogramFirstProcessingStartToLastProcessingEndPerAnimationFrame,
          histogram_suffix, total_processing_duration);

      base::TimeDelta total_accountable_processing_duration =
          TotalNonOverlappingProcessingDuration(event_timing_entries_);
      base::TimeDelta total_unaccountable_processing_duration =
          total_processing_duration - total_accountable_processing_duration;
      ReportPerAnimationFrameHistograms(
          kHistogramTotalUnaccountedEventProcessingTimePerAnimationFrame,
          histogram_suffix, total_unaccountable_processing_duration);

      // Presentation delay breakdown.
      if (!last_event_presentation_time.is_null() &&
          !last_event_commit_finish_time.is_null() &&
          !last_event_render_start_time.is_null()) {
        ReportPerAnimationFrameHistograms(
            kHistogramProcessingEndToRenderStartTimePerAnimationFrame,
            histogram_suffix,
            last_event_render_start_time - last_event_processing_end_time);
        ReportPerAnimationFrameHistograms(
            kHistogramRenderStartTimeToCommitTimePerAnimationFrame,
            histogram_suffix,
            last_event_commit_finish_time - last_event_render_start_time);
        ReportPerAnimationFrameHistograms(
            kHistogramCommitToPresentationTimePerAnimationFrame,
            histogram_suffix,
            last_event_presentation_time - last_event_commit_finish_time);
        ReportPerAnimationFrameHistograms(
            kHistogramProcessingEndToPresentationTimePerAnimationFrame,
            histogram_suffix,
            last_event_presentation_time - last_event_processing_end_time);

        // Overall durations
        ReportPerAnimationFrameHistograms(
            kHistogramEventQueueTimeToCommitPerAnimationFrame, histogram_suffix,
            last_event_commit_finish_time -
                first_event_enqueued_to_main_thread_time);
        ReportPerAnimationFrameHistograms(
            kHistogramEventCreationToPresentationTimePerAnimationFrame,
            histogram_suffix,
            last_event_presentation_time - first_event_creation_time);
      } else {
        ReportPerAnimationFrameHistograms(
            kHistogramEventCreationToLastProcessingEndPerAnimationFrame,
            histogram_suffix,
            last_event_processing_end_time - first_event_creation_time);
      }
    }

    // Remove reported EventData objects.
    event_timing_entries_.erase(first, last);
  }
}

void WindowPerformance::ReportEvent(
    InteractiveDetector* interactive_detector,
    Member<PerformanceEventTiming> event_timing_entry) {
  auto* timings = event_timing_entry->GetEventTimingReportingInfo();
  base::TimeTicks event_creation_time = timings->creation_time;
  base::TimeTicks enqueued_to_main_thread_time =
      timings->enqueued_to_main_thread_time;
  base::TimeTicks processing_start = timings->processing_start_time;
  base::TimeTicks processing_end = timings->processing_end_time;
  base::TimeDelta processing_duration = processing_end - processing_start;
  base::TimeTicks event_end_time = event_timing_entry->GetEndTime();
  base::TimeTicks commit_or_end_time = timings->commit_finish_time.is_null()
                                           ? event_end_time
                                           : timings->commit_finish_time;
  base::TimeDelta time_to_next_paint = event_end_time - processing_end;

  // event_creation_time might be null in certain tests.
  // CHECK(!event_creation_time.is_null());
  CHECK(!processing_start.is_null());
  CHECK(!processing_end.is_null());
  CHECK(!event_end_time.is_null());
  CHECK(timings->fallback_time.is_null() ||
        timings->fallback_time == event_end_time);

  // Round to 8ms.
  int rounded_duration =
      std::round((event_end_time - event_creation_time).InMillisecondsF() / 8) *
      8;

  event_timing_entry->SetDuration(rounded_duration);

  if (event_timing_entry->name() == "pointerdown") {
    pending_pointer_down_processing_time_ = processing_duration;
    pending_pointer_down_time_to_next_paint_ = time_to_next_paint;
  } else if (event_timing_entry->name() == "pointerup") {
    if (pending_pointer_down_time_to_next_paint_.has_value() &&
        interactive_detector) {
      interactive_detector->RecordInputEventTimingUMA(
          pending_pointer_down_processing_time_.value(),
          pending_pointer_down_time_to_next_paint_.value());
    }
  } else if ((event_timing_entry->name() == "click" ||
              event_timing_entry->name() == "keydown" ||
              event_timing_entry->name() == "mousedown") &&
             interactive_detector) {
    interactive_detector->RecordInputEventTimingUMA(processing_duration,
                                                    time_to_next_paint);
  }

  // Event Timing
  ResponsivenessMetrics::EventTimestamps event_timestamps = {
      event_creation_time, enqueued_to_main_thread_time, commit_or_end_time,
      event_end_time};

  if (SetInteractionIdAndRecordLatency(event_timing_entry, event_timestamps)) {
    NotifyAndAddEventTimingBuffer(event_timing_entry);
  }

  ReportFirstInputTiming(event_timing_entry);
}

void WindowPerformance::ReportFirstInputTiming(
    PerformanceEventTiming* event_timing_entry) {
  // First Input
  //
  // See also ./First_input_state_machine.md
  // (https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/timing/First_input_state_machine.md)
  // to understand the logics below.
  if (!first_input_timing_) {
    if (event_timing_entry->name() == event_type_names::kPointerdown) {
      first_pointer_down_event_timing_ =
          PerformanceEventTiming::CreateFirstInputTiming(event_timing_entry);
    } else if (event_timing_entry->name() == event_type_names::kPointerup &&
               first_pointer_down_event_timing_) {
      if (event_timing_entry->HasKnownInteractionID()) {
        first_pointer_down_event_timing_->SetInteractionIdAndOffset(
            event_timing_entry->interactionId(),
            event_timing_entry->interactionOffset());
      }
      DispatchFirstInputTiming(first_pointer_down_event_timing_);
    } else if (event_timing_entry->name() == event_type_names::kPointercancel) {
      first_pointer_down_event_timing_.Clear();
    } else if ((event_timing_entry->name() == event_type_names::kMousedown ||
                event_timing_entry->name() == event_type_names::kClick ||
                event_timing_entry->name() == event_type_names::kKeydown) &&
               !first_pointer_down_event_timing_) {
      DispatchFirstInputTiming(
          PerformanceEventTiming::CreateFirstInputTiming(event_timing_entry));
    }
  }
}

void WindowPerformance::NotifyAndAddEventTimingBuffer(
    PerformanceEventTiming* entry) {
  CHECK(entry->HasKnownInteractionID());
  if (HasObserverFor(PerformanceEntry::kEvent)) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingExplicitlyRequested);
    NotifyObserversOfEntry(*entry);
  }

  // TODO(npm): is 104 a reasonable buffering threshold or should it be
  // relaxed?
  if (entry->duration() >= PerformanceObserver::kDefaultDurationThreshold) {
    AddToEventTimingBuffer(*entry);
  }

  bool latency_tracing_enabled = TRACE_EVENT_CATEGORY_ENABLED("latency");
  bool devtools_tracing_enabled =
      TRACE_EVENT_CATEGORY_ENABLED("devtools.timeline");
  const auto parent_track =
      perfetto::NamedTrack::ThreadScoped("EventTimingsByAnimationFrame", this);

  if (latency_tracing_enabled || devtools_tracing_enabled) {
    auto* entryInfo = entry->GetEventTimingReportingInfo();
    auto flow_id = perfetto::Flow::FromPointer(entry);

    TRACE_EVENT_INSTANT("latency", "EventCreation", parent_track,
                        entryInfo->creation_time, flow_id);
    auto enqueued_to_main_thread_time = entryInfo->enqueued_to_main_thread_time;
    if (!enqueued_to_main_thread_time.is_null()) {
      TRACE_EVENT_INSTANT("latency", "EventEnqueuedToMainThread", parent_track,
                          enqueued_to_main_thread_time, flow_id);
    } else {
      // TODO(crbug.com/422215352): Add a Histogram to report the event name
      // when `enqueued_to_main_thread_time` is null.  All events should have
      // this timestamp set-- but we're not observing some forms of event
      // dispatch for which we support EventTiming.  This might be due to IME.
    }

    TRACE_EVENT_BEGIN(
        "latency", "EventProcessing", parent_track,
        entryInfo->processing_start_time, flow_id, "fallback_reason",
        PerformanceEventTiming::FallbackReasonToString(
            entryInfo->fallback_reason),
        "fallback_time", entryInfo->fallback_time,
        [&](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_event_timing();
          entry->SetPerfettoData(DomWindow()->GetFrame(), data,
                                 GetTimeOriginInternal());
        });
    TRACE_EVENT_END("latency", parent_track, entryInfo->processing_end_time);

    TRACE_EVENT_INSTANT("latency", "EventEndTime", parent_track,
                        entry->GetEndTime(), flow_id);

    // Add EventTimingMeasurementComplete trace event to report when Event
    // Timing was measured and reported to the Performance Timeline. This helps
    // track the delay between frame presentation and timeline reporting.
    TRACE_EVENT_INSTANT("latency", "EventTimingMeasurementComplete",
                        parent_track, base::TimeTicks::Now(), flow_id);

    // TODO(sullivan): Remove these events when DevTools migrates to the above
    // perfetto events.
    unsigned hash = GetHash(entry->name());
    AddFloatToHash(hash, entry->startTime());
    TRACE_EVENT_BEGIN("devtools.timeline", "EventTiming",
                      perfetto::Track::Global(hash), entryInfo->creation_time,
                      "data", entry->ToTracedValue(DomWindow()->GetFrame()));

    TRACE_EVENT_END("devtools.timeline", perfetto::Track::Global(hash),
                    entry->GetEndTime());
  }
}

void WindowPerformance::SetHasContainerTimingChanges() {
  DCHECK(RuntimeEnabledFeatures::ContainerTimingEnabled());

  has_container_timing_changes_ = true;
  NotifyObserversOfContainerTiming();
}

void WindowPerformance::PopulateContainerTimingEntries() {
  if (!has_container_timing_changes_) {
    return;
  }

  DCHECK(RuntimeEnabledFeatures::ContainerTimingEnabled());

  LocalDOMWindow* window = DomWindow();
  if (!window) {
    return;
  }

  ContainerTiming& container_timing = ContainerTiming::From(*window);

  container_timing.EmitPerformanceEntries();

  has_container_timing_changes_ = false;
}

bool WindowPerformance::SetInteractionIdAndRecordLatency(
    PerformanceEventTiming* entry,
    ResponsivenessMetrics::EventTimestamps event_timestamps) {
  if (!IsEventTypeForInteractionId(entry->name())) {
    // Set 0 interaction id for event timings that do not go into state
    // machine.
    entry->SetInteractionId(0);
    return true;
  }
  // We set the interactionId and record the metric in the
  // same logic, so we need to ignore the return value when InteractionId is
  // disabled.
  if (entry->GetEventTimingReportingInfo()->pointer_id.has_value()) {
    return responsiveness_metrics_->SetPointerIdAndRecordLatency(
        entry, event_timestamps);
  }

  responsiveness_metrics_->SetKeyIdAndRecordLatency(entry, event_timestamps);

  return true;
}

void WindowPerformance::QueueLongAnimationFrameTiming(
    AnimationFrameTimingInfo* info,
    std::optional<DOMPaintTimingInfo> paint_timing_info) {
  if (auto* window = DomWindow()) {
    AddLongAnimationFrameEntry(PerformanceLongAnimationFrameTiming::Create(
        info, time_origin_, cross_origin_isolated_capability_, window,
        paint_timing_info, NavigationId()));
  }
}

void WindowPerformance::AddFirstPaintTiming(
    const DOMPaintTimingInfo& paint_timing_info) {
  AddPaintTiming(PerformancePaintTiming::PaintType::kFirstPaint,
                 paint_timing_info);
}

void WindowPerformance::AddFirstContentfulPaintTiming(
    const DOMPaintTimingInfo& paint_timing_info) {
  AddPaintTiming(PerformancePaintTiming::PaintType::kFirstContentfulPaint,
                 paint_timing_info);
}

void WindowPerformance::AddLongAnimationFrameEntry(PerformanceEntry* entry) {
  if (!IsLongAnimationFrameBufferFull()) {
    InsertEntryIntoSortedBuffer(long_animation_frame_buffer_, *entry,
                                kRecordSwaps);
  }

  NotifyObserversOfEntry(*entry);
}

void WindowPerformance::AddElementTiming(
    const AtomicString& name,
    const String& url,
    const gfx::RectF& rect,
    const DOMPaintTimingInfo& paint_timing_info,
    base::TimeTicks load_time,
    const AtomicString& identifier,
    const gfx::Size& intrinsic_size,
    const AtomicString& id,
    Element* element) {
  if (!DomWindow()) {
    return;
  }

  DOMHighResTimeStamp coarsened_load_time =
      MonotonicTimeToDOMHighResTimeStamp(load_time);

  PerformanceElementTiming* entry = PerformanceElementTiming::Create(
      name, url, rect, paint_timing_info.presentation_time, coarsened_load_time,
      identifier, intrinsic_size.width(), intrinsic_size.height(), id, element,
      DomWindow(), NavigationId());
  TRACE_EVENT2("loading", "PerformanceElementTiming", "data",
               entry->ToTracedValue(), "frame",
               GetFrameIdForTracing(DomWindow()->GetFrame()));
  entry->SetPaintTimingInfo(paint_timing_info);
  if (HasObserverFor(PerformanceEntry::kElement)) {
    NotifyObserversOfEntry(*entry);
  }
  if (!IsElementTimingBufferFull()) {
    AddToElementTimingBuffer(*entry);
  }
}

void WindowPerformance::AddContainerTiming(
    const DOMPaintTimingInfo& paint_timing_info,
    const gfx::Rect& rect,
    uint64_t size,
    Element* root_element,
    const AtomicString& identifier,
    Element* last_painted_element,
    const DOMPaintTimingInfo& first_paint_timing_info) {
  DCHECK(RuntimeEnabledFeatures::ContainerTimingEnabled());
  if (!DomWindow()) {
    return;
  }

  PerformanceContainerTiming* entry = PerformanceContainerTiming::Create(
      AtomicString("container-paints"), paint_timing_info.presentation_time,
      rect, size, root_element, identifier, last_painted_element,
      first_paint_timing_info.presentation_time, DomWindow(), NavigationId());
  TRACE_EVENT2("loading", "PerformanceContainerTiming", "data",
               entry->ToTracedValue(), "frame",
               GetFrameIdForTracing(DomWindow()->GetFrame()));
  entry->SetPaintTimingInfo(paint_timing_info);
  if (HasObserverFor(PerformanceEntry::kContainer)) {
    NotifyObserversOfContainerEntry(*entry);
  }
  if (!IsContainerTimingBufferFull()) {
    AddToContainerTimingBuffer(*entry);
  }
}

void WindowPerformance::DispatchFirstInputTiming(
    PerformanceEventTiming* entry) {
  if (!entry) {
    return;
  }
  DCHECK_EQ("first-input", entry->entryType());
  if (HasObserverFor(PerformanceEntry::kFirstInput)) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingExplicitlyRequested);
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingFirstInputExplicitlyRequested);
    NotifyObserversOfEntry(*entry);
  }

  DCHECK(!first_input_timing_);
  first_input_timing_ = entry;
}

void WindowPerformance::AddLayoutShiftEntry(LayoutShift* entry) {
  if (HasObserverFor(PerformanceEntry::kLayoutShift)) {
    NotifyObserversOfEntry(*entry);
  }
  AddToLayoutShiftBuffer(*entry);
}

void WindowPerformance::AddVisibilityStateEntry(bool is_visible,
                                                base::TimeTicks timestamp) {
  VisibilityStateEntry* entry = MakeGarbageCollected<VisibilityStateEntry>(
      PageHiddenStateString(!is_visible),
      MonotonicTimeToDOMHighResTimeStamp(timestamp), DomWindow(),
      NavigationId());

  if (HasObserverFor(PerformanceEntry::kVisibilityState)) {
    NotifyObserversOfEntry(*entry);
  }

  if (visibility_state_buffer_.size() < kDefaultVisibilityStateEntrySize) {
    visibility_state_buffer_.push_back(entry);
  }
}

void WindowPerformance::AddSoftNavigationEntry(
    const AtomicString& name,
    base::TimeTicks timestamp,
    const DOMPaintTimingInfo& paint_timing_info,
    uint32_t navigation_id) {
  if (!RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(
          GetExecutionContext())) {
    return;
  }
  SoftNavigationEntry* entry = MakeGarbageCollected<SoftNavigationEntry>(
      name, MonotonicTimeToDOMHighResTimeStamp(timestamp), paint_timing_info,
      DomWindow(), navigation_id);

  if (HasObserverFor(PerformanceEntry::kSoftNavigation)) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kSoftNavigationHeuristics);
    NotifyObserversOfEntry(*entry);
  }

  AddSoftNavigationToPerformanceTimeline(entry);
}

void WindowPerformance::PageVisibilityChanged() {
  PageVisibilityChangedWithTimestamp(base::TimeTicks::Now());
}

void WindowPerformance::PageVisibilityChangedWithTimestamp(
    base::TimeTicks visibility_change_timestamp) {
  // Only flush event timing data when page visibility changes from visible to
  // invisible.
  if (!GetPage()->IsPageVisible()) {
    last_hidden_timestamp_ = visibility_change_timestamp;
    FlushEventTimingsOnPageHidden();
  }
  AddVisibilityStateEntry(GetPage()->IsPageVisible(),
                          visibility_change_timestamp);
}

void WindowPerformance::WillShowModalDialog() {
  show_modal_dialog_timestamps_.push_back(base::TimeTicks::Now());
}

EventCounts* WindowPerformance::eventCounts() {
  if (!event_counts_) {
    event_counts_ = MakeGarbageCollected<EventCounts>();
  }
  return event_counts_.Get();
}

uint64_t WindowPerformance::interactionCount() const {
  return responsiveness_metrics_->GetInteractionCount();
}

void WindowPerformance::OnLargestContentfulPaintUpdated(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t paint_size,
    base::TimeTicks load_time,
    const AtomicString& id,
    const String& url,
    Element* element) {
  auto* entry = MakeGarbageCollected<LargestContentfulPaint>(
      /*start_time=*/paint_timing_info.presentation_time,
      /*render_time=*/paint_timing_info.presentation_time, paint_size,
      MonotonicTimeToDOMHighResTimeStamp(load_time), id, url, element,
      DomWindow(), NavigationId());
  entry->SetPaintTimingInfo(paint_timing_info);

  if (HasObserverFor(PerformanceEntry::kLargestContentfulPaint)) {
    NotifyObserversOfEntry(*entry);
  }
  AddLargestContentfulPaint(entry);
  if (LocalDOMWindow* window = DomWindow()) {
    window->document()->OnLargestContentfulPaintUpdated();
  }

  if (HTMLImageElement* image_element = DynamicTo<HTMLImageElement>(element)) {
    image_element->SetIsLCPElement();
    if (image_element->HasLazyLoadingAttribute()) {
      element->GetDocument().CountUse(WebFeature::kLCPImageWasLazy);
    }
  }

  if (element) {
    if (LocalFrame* local_frame = element->GetDocument().GetFrame()) {
      if (LCPCriticalPathPredictor* lcpp = local_frame->GetLCPP()) {
        std::optional<KURL> maybe_url = std::nullopt;
        if (!url.empty()) {
          maybe_url = KURL(url);
        }
        lcpp->OnLargestContentfulPaintUpdated(*element, maybe_url);
      }
    }
  }
}

void WindowPerformance::OnInteractionContentfulPaintUpdated(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t paint_size,
    base::TimeTicks load_time,
    const AtomicString& id,
    const String& url,
    Element* element,
    uint32_t navigation_id) {
  if (!RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(
          GetExecutionContext())) {
    return;
  }
  auto* entry = MakeGarbageCollected<InteractionContentfulPaint>(
      /*start_time=*/paint_timing_info.presentation_time,
      /*render_time=*/paint_timing_info.presentation_time, paint_size,
      MonotonicTimeToDOMHighResTimeStamp(load_time), id, url, element,
      DomWindow(), navigation_id);
  entry->SetPaintTimingInfo(paint_timing_info);

  if (HasObserverFor(PerformanceEntry::kInteractionContentfulPaint)) {
    NotifyObserversOfEntry(*entry);
  }
  AddInteractionContentfulPaint(entry);
}

void WindowPerformance::OnPaintFinished() {
  // The event processed after a paint will have different presentation time
  // than previous ones, so we need to register a new presentation promise for
  // it.
  need_new_promise_for_event_presentation_time_ = true;
}

void WindowPerformance::OnBeginMainFrame(viz::BeginFrameId frame_id) {
  const uint64_t source_id = frame_id.source_id;
  if (source_id) {
    begin_main_frame_source_id_ = source_id;
  }
}

void WindowPerformance::OnPageScroll() {
  autoscroll_active_ =
      GetPage()->GetAutoscrollController().SelectionAutoscrollInProgress();
}

bool WindowPerformance::IsAutoscrollActive() {
  return autoscroll_active_;
}

}  // namespace blink
