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
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
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
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_hidden_state.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_long_animation_frame_timing.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
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
  NOTREACHED_IN_MIGRATION();
  return g_empty_atom;
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

}  // namespace

constexpr size_t kDefaultVisibilityStateEntrySize = 50;

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
             "AdjustNavigationalPrefetchTiming",
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
      *DomWindow(), std::move(info), time_origin_);
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

void WindowPerformance::RegisterEventTiming(const Event& event,
                                            EventTarget* event_target,
                                            base::TimeTicks start_time,
                                            base::TimeTicks processing_start,
                                            base::TimeTicks processing_end) {
  // |start_time| could be null in some tests that inject input.
  DCHECK(!processing_start.is_null());
  DCHECK(!processing_end.is_null());
  DCHECK_GE(processing_end, processing_start);
  if (!DomWindow() || !DomWindow()->GetFrame()) {
    return;
  }

  const AtomicString& event_type = event.type();
  const PointerEvent* pointer_event = DynamicTo<PointerEvent>(event);
  if (event_type == event_type_names::kPointermove) {
    // A trusted pointermove must be a PointerEvent.
    if (!event.IsPointerEvent()) {
      return;
    }

    NotifyPotentialDrag(pointer_event->pointerId());
    SetCurrentEventTimingEvent(nullptr);
    return;
  }
  eventCounts()->Add(event_type);

  if (need_new_promise_for_event_presentation_time_) {
    DomWindow()->GetFrame()->GetChromeClient().NotifyPresentationTime(
        *DomWindow()->GetFrame(),
        CrossThreadBindOnce(&WindowPerformance::OnPresentationPromiseResolved,
                            WrapCrossThreadWeakPersistent(this),
                            ++event_presentation_promise_count_));
    need_new_promise_for_event_presentation_time_ = false;
  }

  std::optional<PointerId> pointer_id;
  if (pointer_event) {
    pointer_id = pointer_event->pointerId();
  }
  std::optional<int> key_code;
  if (event.IsKeyboardEvent()) {
    key_code = DynamicTo<KeyboardEvent>(event)->keyCode();
  }

  PerformanceEventTiming::EventTimingReportingInfo reporting_info{
      .presentation_index = event_presentation_promise_count_,
      .creation_time = start_time,
      .enqueued_to_main_thread_time =
          responsiveness_metrics_->CurrentInteractionEventQueuedTimestamp(),
      .processing_start_time = processing_start,
      .processing_end_time = processing_end,
      .key_code = key_code,
      .pointer_id = pointer_id,
      .prevent_counting_as_interaction =
          pointer_event ? pointer_event->GetPreventCountingAsInteraction()
                        : false};

  PerformanceEventTiming* entry = PerformanceEventTiming::Create(
      event_type, reporting_info, event.cancelable(),
      event_target ? event_target->ToNode() : nullptr,
      DomWindow());  // TODO(haoliuk): Add WPT for Event Timing.
                     // See crbug.com/1320878.

  // Add |entry| to in the order of processing_start, along with the
  // presentation promise index in order to match with corresponding
  // presentation feedback later.

  auto reverse_iter = std::find_if_not(
      event_timing_entries_.rbegin(), event_timing_entries_.rend(),
      [processing_start](auto event) {
        return processing_start <
               event->GetEventTimingReportingInfo()->processing_start_time;
      });
  event_timing_entries_.InsertAt(reverse_iter.base(), entry);

  SetCurrentEventTimingEvent(nullptr);
}

void WindowPerformance::SetCommitFinishTimeStampForPendingEvents(
    base::TimeTicks commit_finish_time) {
  for (auto entry : event_timing_entries_) {
    // Skip events that don't need paint, or have already been painted
    if (entry->GetEventTimingReportingInfo()->commit_finish_time.has_value()) {
      continue;
    }
    if (entry->HasKnownEndTime()) {
      continue;
    }
    entry->GetEventTimingReportingInfo()->commit_finish_time =
        commit_finish_time;
  }
}

// Parameters:
// |presentation_index|     - The registering index of the presentation promise.
//                            First registered presentation promise will have an
//                            index of 1.
// |presentation_timestamp| - The frame presenting time or an early exit time
//                            due to no frame updates.
void WindowPerformance::OnPresentationPromiseResolved(
    uint64_t presentation_index,
    const viz::FrameTimingDetails& presentation_details) {
  if (!DomWindow() || !DomWindow()->document()) {
    return;
  }

  // If the resolved presentation promise is the latest one we registered, then
  // events arrive after will need a new presentation promise to provide
  // presentation feedback.
  if (presentation_index == event_presentation_promise_count_) {
    need_new_promise_for_event_presentation_time_ = true;
  }

  base::TimeTicks presentation_timestamp =
      presentation_details.presentation_feedback.timestamp;
  for (auto event_timing_entry : event_timing_entries_) {
    if (event_timing_entry->GetEventTimingReportingInfo()->presentation_index ==
        presentation_index) {
      event_timing_entry->GetEventTimingReportingInfo()->presentation_time =
          presentation_timestamp;
    }
  }
  ReportEventTimings();
}

void WindowPerformance::FlushEventTimingsOnPageHidden() {
  ReportAllPendingEventTimingsOnPageHidden();

  // Remove any remaining events that are not flushed by the above step.
  responsiveness_metrics_->FlushAllEventsAtPageHidden();
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
  // Ideally the fallback time could be the last_hidden_timestamp_, but we don't
  // actually have an accurate value for that (it would need to come from
  // browser IPC).
  for (auto event_timing_entry : event_timing_entries_) {
    if (!event_timing_entry->HasKnownEndTime()) {
      event_timing_entry->GetEventTimingReportingInfo()->fallback_time =
          event_timing_entry->GetEventTimingReportingInfo()
              ->processing_end_time;
    }
  }
  ReportEventTimings();
}

void WindowPerformance::ReportEventTimings() {
  CHECK(DomWindow() && DomWindow()->document());
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*(DomWindow()->document()));

  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("devtools.timeline", &tracing_enabled);

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
                     [](auto entry) { return entry->HasKnownEndTime(); })) {
      break;
    }

    if (tracing_enabled) {
      auto scope = perfetto::Track::ThreadScoped(this);
      auto flowid = perfetto::Flow::ProcessScoped(presentation_index);

      auto* first_event_reporting_info =
          first->Get()->GetEventTimingReportingInfo();
      auto frame_start_time = first_event_reporting_info->processing_start_time;

      TRACE_EVENT_BEGIN("devtools.timeline", "EventsInAnimationFrame", scope,
                        frame_start_time, flowid);

      TRACE_EVENT_INSTANT("devtools.timeline", "EventCreation", scope,
                          first_event_reporting_info->creation_time, flowid);
    }

    // Report all the events in this frame
    std::for_each(first, last, [&](auto entry) {
      ReportEvent(interactive_detector, entry);
    });

    if (tracing_enabled) {
      auto scope = perfetto::Track::ThreadScoped(this);
      auto flowid = perfetto::Flow::ProcessScoped(presentation_index);

      auto* last_event_reporting_info =
          std::prev(last)->Get()->GetEventTimingReportingInfo();
      auto frame_end_time =
          last_event_reporting_info->commit_finish_time.value_or(
              last_event_reporting_info->processing_end_time);

      TRACE_EVENT_END("devtools.timeline", scope, frame_end_time);

      if (last_event_reporting_info->presentation_time.has_value()) {
        TRACE_EVENT_INSTANT(
            "devtools.timeline", "EventPresentation", scope,
            last_event_reporting_info->presentation_time.value(), flowid);
      }

      if (auto first_entry_with_fallback =
              std::find_if(first, last,
                           [](auto entry) {
                             return entry->GetEventTimingReportingInfo()
                                 ->fallback_time.has_value();
                           });
          first_entry_with_fallback != last) {
        TRACE_EVENT_INSTANT("devtools.timeline", "EventFallbackTime", scope,
                            first_entry_with_fallback->Get()
                                ->GetEventTimingReportingInfo()
                                ->fallback_time.value(),
                            flowid);
      }
    }

    // Remove reported EventData objects.
    event_timing_entries_.erase(first, last);
  }
}

void WindowPerformance::ReportEvent(
    InteractiveDetector* interactive_detector,
    Member<PerformanceEventTiming> event_timing_entry) {
  base::TimeTicks event_creation_time =
      event_timing_entry->GetEventTimingReportingInfo()->creation_time;
  base::TimeTicks processing_start =
      event_timing_entry->GetEventTimingReportingInfo()->processing_start_time;
  base::TimeTicks processing_end =
      event_timing_entry->GetEventTimingReportingInfo()->processing_end_time;
  SetFallbackTime(event_timing_entry);

  base::TimeTicks event_end_time = event_timing_entry->GetEndTime();

  base::TimeDelta time_to_next_paint = event_end_time - processing_end;

  // Round to 8ms.
  int rounded_duration =
      std::round((event_end_time - event_creation_time).InMillisecondsF() / 8) *
      8;

  event_timing_entry->SetDuration(rounded_duration);

  base::TimeDelta processing_duration = processing_end - processing_start;

  if (event_timing_entry->name() == "pointerdown") {
    pending_pointer_down_start_time_ = event_timing_entry->startTime();

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
      event_creation_time,
      event_timing_entry->GetEventTimingReportingInfo()
          ->enqueued_to_main_thread_time,
      event_timing_entry->GetEventTimingReportingInfo()
          ->commit_finish_time.value_or(
              event_timing_entry->GetEventTimingReportingInfo()
                  ->processing_end_time),
      event_end_time};
  if (SetInteractionIdAndRecordLatency(event_timing_entry, event_timestamps)) {
    NotifyAndAddEventTimingBuffer(event_timing_entry);
  }

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

  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("devtools.timeline", &tracing_enabled);

  if (tracing_enabled) {
    base::TimeTicks unsafe_start_time =
        entry->GetEventTimingReportingInfo()->creation_time;
    base::TimeTicks unsafe_end_time = entry->GetEndTime();
    unsigned hash = WTF::GetHash(entry->name());
    WTF::AddFloatToHash(hash, entry->startTime());
    auto track_id = perfetto::Track::ThreadScoped(this);
    auto flow_id = perfetto::Flow::FromPointer(entry);
    TRACE_EVENT_INSTANT("devtools.timeline", "EventCreation", track_id,
                        entry->GetEventTimingReportingInfo()->creation_time,
                        flow_id);
    TRACE_EVENT_INSTANT(
        "devtools.timeline", "EventEnqueuedToMainThread", track_id,
        entry->GetEventTimingReportingInfo()->enqueued_to_main_thread_time,
        flow_id);

    TRACE_EVENT_BEGIN(
        "devtools.timeline", "EventProcessing", track_id,
        entry->GetEventTimingReportingInfo()->processing_start_time, flow_id,
        [&](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_event_timing();
          entry->SetPerfettoData(DomWindow()->GetFrame(), data,
                                 GetTimeOriginInternal());
        });
    TRACE_EVENT_END("devtools.timeline", track_id,
                    entry->GetEventTimingReportingInfo()->processing_end_time);

    // TODO(sullivan): Remove these events when DevTools migrates to the above
    // perfetto events.
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "devtools.timeline", "EventTiming", hash, unsafe_start_time, "data",
        entry->ToTracedValue(DomWindow()->GetFrame()));

    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "devtools.timeline", "EventTiming", hash, unsafe_end_time);
  }
}

void WindowPerformance::SetFallbackTime(PerformanceEventTiming* entry) {
  if (entry->GetEventTimingReportingInfo()->fallback_time.has_value()) {
    return;
  }
  // For artificial events on MacOS, we will fallback entry's end time to its
  // processingEnd (as if there was no next paint needed). crbug.com/1321819.
  const bool is_artificial_pointerup_or_click =
      (entry->name() == event_type_names::kPointerup ||
       entry->name() == event_type_names::kClick) &&
      entry->startTime() == pending_pointer_down_start_time_;

  if (is_artificial_pointerup_or_click) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingArtificialPointerupOrClick);
  }

  // If the page visibility was changed. We fallback entry's end time to its
  // processingEnd (as if there was no next paint needed). crbug.com/1312568.
  bool was_page_visibility_changed =
      last_hidden_timestamp_ >
          entry->GetEventTimingReportingInfo()->creation_time &&
      last_hidden_timestamp_ <
          entry->GetEventTimingReportingInfo()->presentation_time;

  // An javascript synchronous modal dialog showed before the event frame
  // got presented. User could wait for arbitrarily long on the dialog. Thus
  // we fall back presentation time to the pre dialog showing time.
  // crbug.com/1435448.
  bool fallback_end_time_to_dialog_time = false;
  base::TimeTicks first_modal_dialog_timestamp;

  // Clean up stale dialog times.
  while (!show_modal_dialog_timestamps_.empty() &&
         show_modal_dialog_timestamps_.front() <
             entry->GetEventTimingReportingInfo()->creation_time) {
    show_modal_dialog_timestamps_.pop_front();
  }

  if (!show_modal_dialog_timestamps_.empty() &&
      show_modal_dialog_timestamps_.front() <
          entry->GetEventTimingReportingInfo()->presentation_time) {
    fallback_end_time_to_dialog_time = true;
    first_modal_dialog_timestamp = show_modal_dialog_timestamps_.front();
  }

  const bool fallback_end_time_to_processing_end =
      was_page_visibility_changed
#if BUILDFLAG(IS_MAC)
      || is_artificial_pointerup_or_click
#endif  // BUILDFLAG(IS_MAC)
      ;

  // Set a fallback time.
  if (fallback_end_time_to_dialog_time && fallback_end_time_to_processing_end) {
    entry->GetEventTimingReportingInfo()->fallback_time =
        std::min(first_modal_dialog_timestamp,
                 entry->GetEventTimingReportingInfo()->processing_end_time);
  } else if (fallback_end_time_to_dialog_time) {
    entry->GetEventTimingReportingInfo()->fallback_time =
        first_modal_dialog_timestamp;
  } else if (fallback_end_time_to_processing_end) {
    entry->GetEventTimingReportingInfo()->fallback_time =
        entry->GetEventTimingReportingInfo()->processing_end_time;
  }
}

bool WindowPerformance::SetInteractionIdAndRecordLatency(
    PerformanceEventTiming* entry,
    ResponsivenessMetrics::EventTimestamps event_timestamps) {
  if (!IsEventTypeForInteractionId(entry->name())) {
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

void WindowPerformance::ReportLongAnimationFrameTiming(
    AnimationFrameTimingInfo* info) {
  LocalDOMWindow* window = DomWindow();
  if (!window) {
    return;
  }

  PerformanceLongAnimationFrameTiming* entry =
      MakeGarbageCollected<PerformanceLongAnimationFrameTiming>(
          info, time_origin_, cross_origin_isolated_capability_, window);

  if (!IsLongAnimationFrameBufferFull()) {
    InsertEntryIntoSortedBuffer(long_animation_frame_buffer_, *entry,
                                kRecordSwaps);
  }

  NotifyObserversOfEntry(*entry);
}

void WindowPerformance::AddElementTiming(const AtomicString& name,
                                         const String& url,
                                         const gfx::RectF& rect,
                                         base::TimeTicks start_time,
                                         base::TimeTicks load_time,
                                         const AtomicString& identifier,
                                         const gfx::Size& intrinsic_size,
                                         const AtomicString& id,
                                         Element* element) {
  if (!DomWindow()) {
    return;
  }
  PerformanceElementTiming* entry = PerformanceElementTiming::Create(
      name, url, rect, MonotonicTimeToDOMHighResTimeStamp(start_time),
      MonotonicTimeToDOMHighResTimeStamp(load_time), identifier,
      intrinsic_size.width(), intrinsic_size.height(), id, element,
      DomWindow());
  TRACE_EVENT2("loading", "PerformanceElementTiming", "data",
               entry->ToTracedValue(), "frame",
               GetFrameIdForTracing(DomWindow()->GetFrame()));
  if (HasObserverFor(PerformanceEntry::kElement)) {
    NotifyObserversOfEntry(*entry);
  }
  if (!IsElementTimingBufferFull()) {
    AddToElementTimingBuffer(*entry);
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
      MonotonicTimeToDOMHighResTimeStamp(timestamp), DomWindow());

  if (HasObserverFor(PerformanceEntry::kVisibilityState)) {
    NotifyObserversOfEntry(*entry);
  }

  if (visibility_state_buffer_.size() < kDefaultVisibilityStateEntrySize) {
    visibility_state_buffer_.push_back(entry);
  }
}

void WindowPerformance::AddSoftNavigationEntry(const AtomicString& name,
                                               base::TimeTicks timestamp) {
  if (!RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(
          GetExecutionContext())) {
    return;
  }
  SoftNavigationEntry* entry = MakeGarbageCollected<SoftNavigationEntry>(
      name, MonotonicTimeToDOMHighResTimeStamp(timestamp), DomWindow());

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

    if (RuntimeEnabledFeaturesBase::
            ReportEventTimingAtVisibilityChangeEnabled()) {
      FlushEventTimingsOnPageHidden();
    }
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
    base::TimeTicks start_time,
    base::TimeTicks render_time,
    uint64_t paint_size,
    base::TimeTicks load_time,
    base::TimeTicks first_animated_frame_time,
    const AtomicString& id,
    const String& url,
    Element* element,
    bool is_triggered_by_soft_navigation) {
  DOMHighResTimeStamp start_timestamp =
      MonotonicTimeToDOMHighResTimeStamp(start_time);
  DOMHighResTimeStamp render_timestamp =
      MonotonicTimeToDOMHighResTimeStamp(render_time);
  DOMHighResTimeStamp load_timestamp =
      MonotonicTimeToDOMHighResTimeStamp(load_time);
  DOMHighResTimeStamp first_animated_frame_timestamp =
      MonotonicTimeToDOMHighResTimeStamp(first_animated_frame_time);
  // TODO(yoav): Should we modify start to represent the animated frame?
  auto* entry = MakeGarbageCollected<LargestContentfulPaint>(
      start_timestamp, render_timestamp, paint_size, load_timestamp,
      first_animated_frame_timestamp, id, url, element, DomWindow(),
      is_triggered_by_soft_navigation);
  if (HasObserverFor(PerformanceEntry::kLargestContentfulPaint)) {
    NotifyObserversOfEntry(*entry);
  }
  AddLargestContentfulPaint(entry);
  if (HTMLImageElement* image_element = DynamicTo<HTMLImageElement>(element)) {
    image_element->SetIsLCPElement();
    if (image_element->HasLazyLoadingAttribute()) {
      element->GetDocument().CountUse(WebFeature::kLCPImageWasLazy);
    }
  }

  if (element) {
    element->GetDocument().OnLargestContentfulPaintUpdated();

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

void WindowPerformance::OnPaintFinished() {
  // The event processed after a paint will have different presentation time
  // than previous ones, so we need to register a new presentation promise for
  // it.
  need_new_promise_for_event_presentation_time_ = true;
}

void WindowPerformance::NotifyPotentialDrag(PointerId pointer_id) {
  responsiveness_metrics_->NotifyPotentialDrag(pointer_id);
}

}  // namespace blink
