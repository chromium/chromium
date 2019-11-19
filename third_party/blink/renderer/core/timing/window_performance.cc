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

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

static constexpr base::TimeDelta kLongTaskObserverThreshold =
    base::TimeDelta::FromMilliseconds(50);

namespace blink {

namespace {

// Events taking longer than this threshold to finish being processed are
// regarded as long-latency events by event-timing. Shorter-latency events are
// ignored to reduce performance impact.
constexpr int kEventTimingDurationThresholdInMs = 104;

String GetFrameAttribute(HTMLFrameOwnerElement* frame_owner,
                         const QualifiedName& attr_name,
                         bool truncate) {
  String attr_value;
  if (frame_owner->hasAttribute(attr_name)) {
    attr_value = frame_owner->getAttribute(attr_name);
    if (truncate && attr_value.length() > 100)
      attr_value = attr_value.Substring(0, 100);  // Truncate to 100 chars
  }
  return attr_value;
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
  if (observer_frame == culprit_frame)
    return SelfKeyword();
  if (observer_frame->Tree().IsDescendantOf(culprit_frame))
    return SameOriginAncestorKeyword();
  if (culprit_frame->Tree().IsDescendantOf(observer_frame))
    return SameOriginDescendantKeyword();
  return SameOriginKeyword();
}

}  // namespace

static base::TimeTicks ToTimeOrigin(LocalDOMWindow* window) {
  Document* document = window->document();
  if (!document)
    return base::TimeTicks();

  DocumentLoader* loader = document->Loader();
  if (!loader)
    return base::TimeTicks();

  return loader->GetTiming().ReferenceMonotonicTime();
}

WindowPerformance::WindowPerformance(LocalDOMWindow* window)
    : Performance(
          ToTimeOrigin(window),
          window->document()->GetTaskRunner(TaskType::kPerformanceTimeline)),
      DOMWindowClient(window) {}

WindowPerformance::~WindowPerformance() = default;

ExecutionContext* WindowPerformance::GetExecutionContext() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetDocument();
}

PerformanceTiming* WindowPerformance::timing() const {
  if (!timing_)
    timing_ = MakeGarbageCollected<PerformanceTiming>(GetFrame());

  return timing_.Get();
}

PerformanceNavigation* WindowPerformance::navigation() const {
  if (!navigation_)
    navigation_ = MakeGarbageCollected<PerformanceNavigation>(GetFrame());

  return navigation_.Get();
}

MemoryInfo* WindowPerformance::memory() const {
  // The performance.memory() API has been improved so that we report precise
  // values when the process is locked to a site. The intent (which changed
  // course over time about what changes would be implemented) can be found at
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/no00RdMnGio,
  // and the relevant bug is https://crbug.com/807651.
  return MakeGarbageCollected<MemoryInfo>(
      Platform::Current()->IsLockedToSite()
          ? MemoryInfo::Precision::Precise
          : MemoryInfo::Precision::Bucketized);
}

PerformanceNavigationTiming*
WindowPerformance::CreateNavigationTimingInstance() {
  if (!GetFrame())
    return nullptr;
  DocumentLoader* document_loader = GetFrame()->Loader().GetDocumentLoader();
  if (!document_loader)
    return nullptr;
  ResourceTimingInfo* info = document_loader->GetNavigationTimingInfo();
  if (!info)
    return nullptr;
  WebVector<WebServerTimingInfo> server_timing =
      PerformanceServerTiming::ParseServerTiming(*info);
  if (!server_timing.empty())
    document_loader->CountUse(WebFeature::kPerformanceServerTiming);

  return MakeGarbageCollected<PerformanceNavigationTiming>(
      GetFrame(), info, time_origin_, server_timing);
}

void WindowPerformance::UpdateLongTaskInstrumentation() {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return;

  if (HasObserverFor(PerformanceEntry::kLongTask)) {
    UseCounter::Count(GetFrame()->GetDocument(), WebFeature::kLongTaskObserver);
    GetFrame()->GetPerformanceMonitor()->Subscribe(
        PerformanceMonitor::kLongTask, kLongTaskObserverThreshold, this);
  } else {
    GetFrame()->GetPerformanceMonitor()->UnsubscribeAll(this);
  }
}

void WindowPerformance::BuildJSONValue(V8ObjectBuilder& builder) const {
  Performance::BuildJSONValue(builder);
  builder.Add("timing", timing()->toJSONForBinding(builder.GetScriptState()));
  builder.Add("navigation",
              navigation()->toJSONForBinding(builder.GetScriptState()));
}

void WindowPerformance::Trace(blink::Visitor* visitor) {
  visitor->Trace(event_timings_);
  visitor->Trace(first_pointer_down_event_timing_);
  visitor->Trace(navigation_);
  visitor->Trace(timing_);
  Performance::Trace(visitor);
  PerformanceMonitor::Client::Trace(visitor);
  DOMWindowClient::Trace(visitor);
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

  Document* document = DynamicTo<Document>(task_context);
  if (!document || !document->GetFrame()) {
    // Unable to attribute as no script was involved.
    DEFINE_STATIC_LOCAL(const AtomicString, kUnknownAttribution, ("unknown"));
    return std::make_pair(kUnknownAttribution, nullptr);
  }

  // Exactly one culprit location, attribute based on origin boundary.
  Frame* culprit_frame = document->GetFrame();
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
  if (!GetFrame())
    return;
  std::pair<AtomicString, DOMWindow*> attribution =
      WindowPerformance::SanitizedAttribution(
          task_context, has_multiple_contexts, GetFrame());
  DOMWindow* culprit_dom_window = attribution.second;
  if (!culprit_dom_window || !culprit_dom_window->GetFrame() ||
      !culprit_dom_window->GetFrame()->DeprecatedLocalOwner()) {
    AddLongTaskTiming(start_time, end_time, attribution.first, g_empty_string,
                      g_empty_string, g_empty_string);
  } else {
    HTMLFrameOwnerElement* frame_owner =
        culprit_dom_window->GetFrame()->DeprecatedLocalOwner();
    AddLongTaskTiming(
        start_time, end_time, attribution.first,
        GetFrameAttribute(frame_owner, html_names::kSrcAttr, false),
        GetFrameAttribute(frame_owner, html_names::kIdAttr, false),
        GetFrameAttribute(frame_owner, html_names::kNameAttr, true));
  }
}

void WindowPerformance::RegisterEventTiming(const AtomicString& event_type,
                                            base::TimeTicks start_time,
                                            base::TimeTicks processing_start,
                                            base::TimeTicks processing_end,
                                            bool cancelable) {
  // |start_time| could be null in some tests that inject input.
  DCHECK(!processing_start.is_null());
  DCHECK(!processing_end.is_null());
  DCHECK_GE(processing_end, processing_start);
  if (!GetFrame())
    return;

  PerformanceEventTiming* entry = PerformanceEventTiming::Create(
      event_type, MonotonicTimeToDOMHighResTimeStamp(start_time),
      MonotonicTimeToDOMHighResTimeStamp(processing_start),
      MonotonicTimeToDOMHighResTimeStamp(processing_end), cancelable);
  event_timings_.push_back(entry);
  // Only queue a swap promise when |event_timings_| was empty. All of the
  // elements in |event_timings_| will be processed in a single call of
  // ReportEventTimings() when the promise suceeds or fails. This method also
  // clears the vector, so a promise has already been queued when the vector was
  // not previously empty.
  if (event_timings_.size() == 1) {
    GetFrame()->GetChromeClient().NotifySwapTime(
        *GetFrame(), CrossThreadBindOnce(&WindowPerformance::ReportEventTimings,
                                         WrapCrossThreadWeakPersistent(this)));
  }
}

void WindowPerformance::ReportEventTimings(WebWidgetClient::SwapResult result,
                                           base::TimeTicks timestamp) {
  DOMHighResTimeStamp end_time = MonotonicTimeToDOMHighResTimeStamp(timestamp);
  bool event_timing_enabled =
      RuntimeEnabledFeatures::EventTimingEnabled(GetExecutionContext());
  for (const auto& entry : event_timings_) {
    int duration_in_ms = std::round((end_time - entry->startTime()) / 8) * 8;
    entry->SetDuration(duration_in_ms);
    if (!first_input_timing_) {
      if (entry->name() == "pointerdown") {
        first_pointer_down_event_timing_ =
            PerformanceEventTiming::CreateFirstInputTiming(entry);
      } else if (entry->name() == "pointerup") {
        DispatchFirstInputTiming(first_pointer_down_event_timing_);
      } else if (entry->name() == "click" || entry->name() == "keydown" ||
                 entry->name() == "mousedown") {
        DispatchFirstInputTiming(
            PerformanceEventTiming::CreateFirstInputTiming(entry));
      }
    }
    if (duration_in_ms < kEventTimingDurationThresholdInMs ||
        !event_timing_enabled)
      continue;

    if (HasObserverFor(PerformanceEntry::kEvent)) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kEventTimingExplicitlyRequested);
      NotifyObserversOfEntry(*entry);
    }

    if (!IsEventTimingBufferFull())
      AddEventTimingBuffer(*entry);
  }
  event_timings_.clear();
}

void WindowPerformance::AddElementTiming(const AtomicString& name,
                                         const String& url,
                                         const FloatRect& rect,
                                         base::TimeTicks start_time,
                                         base::TimeTicks load_time,
                                         const AtomicString& identifier,
                                         const IntSize& intrinsic_size,
                                         const AtomicString& id,
                                         Element* element) {
  PerformanceElementTiming* entry = PerformanceElementTiming::Create(
      name, url, rect, MonotonicTimeToDOMHighResTimeStamp(start_time),
      MonotonicTimeToDOMHighResTimeStamp(load_time), identifier,
      intrinsic_size.Width(), intrinsic_size.Height(), id, element);
  if (HasObserverFor(PerformanceEntry::kElement))
    NotifyObserversOfEntry(*entry);
  if (!IsElementTimingBufferFull())
    AddElementTimingBuffer(*entry);
}

void WindowPerformance::DispatchFirstInputTiming(
    PerformanceEventTiming* entry) {
  if (!entry)
    return;
  DCHECK_EQ("first-input", entry->entryType());
  if (HasObserverFor(PerformanceEntry::kFirstInput)) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingExplicitlyRequested);
    NotifyObserversOfEntry(*entry);
  }

  DCHECK(!first_input_timing_);
  first_input_timing_ = entry;
}

void WindowPerformance::AddLayoutShiftValue(double value,
                                            bool input_detected,
                                            base::TimeTicks input_timestamp) {
  auto* entry = MakeGarbageCollected<LayoutShift>(
      now(), value, input_detected,
      input_detected ? MonotonicTimeToDOMHighResTimeStamp(input_timestamp)
                     : 0.0);
  if (HasObserverFor(PerformanceEntry::kLayoutShift))
    NotifyObserversOfEntry(*entry);
  AddLayoutShiftBuffer(*entry);
}

void WindowPerformance::OnLargestContentfulPaintUpdated(
    base::TimeTicks paint_time,
    uint64_t paint_size,
    base::TimeTicks load_time,
    const AtomicString& id,
    const String& url,
    Element* element) {
  double render_timestamp = MonotonicTimeToDOMHighResTimeStamp(paint_time);
  double load_timestamp = MonotonicTimeToDOMHighResTimeStamp(load_time);
  double start_timestamp =
      render_timestamp != 0.0 ? render_timestamp : load_timestamp;
  auto* entry = MakeGarbageCollected<LargestContentfulPaint>(
      start_timestamp, render_timestamp, paint_size, load_timestamp, id, url,
      element);
  if (HasObserverFor(PerformanceEntry::kLargestContentfulPaint))
    NotifyObserversOfEntry(*entry);
  AddLargestContentfulPaint(entry);
}

}  // namespace blink
