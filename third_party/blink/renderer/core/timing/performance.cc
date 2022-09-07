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

#include "third_party/blink/renderer/core/timing/performance.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_mark_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_measure_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_init_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_performancemeasureoptions_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/back_forward_cache_restoration.h"
#include "third_party/blink/renderer/core/timing/background_tracing_helper.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_controller.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_long_task_timing.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"
#include "third_party/blink/renderer/core/timing/performance_server_timing.h"
#include "third_party/blink/renderer/core/timing/performance_user_timing.h"
#include "third_party/blink/renderer/core/timing/profiler.h"
#include "third_party/blink/renderer/core/timing/profiler_group.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8-metrics.h"

namespace blink {

namespace {

// LongTask API can be a source of many events. Filter on Performance object
// level before reporting to UKM to smooth out recorded events over all pages.
constexpr size_t kLongTaskUkmSampleInterval = 100;

const SecurityOrigin* GetSecurityOrigin(ExecutionContext* context) {
  if (context)
    return context->GetSecurityOrigin();
  return nullptr;
}

bool IsMeasureOptionsEmpty(const PerformanceMeasureOptions& options) {
  return !options.hasDetail() && !options.hasEnd() && !options.hasStart() &&
         !options.hasDuration();
}

void RecordLongTaskUkm(ExecutionContext* execution_context,
                       base::TimeDelta start_time,
                       base::TimeDelta duration) {
  v8::metrics::LongTaskStats stats =
      v8::metrics::LongTaskStats::Get(execution_context->GetIsolate());
  // TODO(cbruni, 1275056): Filter out stats without v8_execute_us.
  ukm::builders::PerformanceAPI_LongTask(execution_context->UkmSourceID())
      .SetStartTime(start_time.InMilliseconds())
      .SetDuration(duration.InMicroseconds())
      .SetDuration_V8_GC(stats.gc_full_atomic_wall_clock_duration_us +
                         stats.gc_full_incremental_wall_clock_duration_us +
                         stats.gc_young_wall_clock_duration_us)
      .SetDuration_V8_GC_Full_Atomic(
          stats.gc_full_atomic_wall_clock_duration_us)
      .SetDuration_V8_GC_Full_Incremental(
          stats.gc_full_incremental_wall_clock_duration_us)
      .SetDuration_V8_GC_Young(stats.gc_young_wall_clock_duration_us)
      .SetDuration_V8_Execute(stats.v8_execute_us)
      .Record(execution_context->UkmRecorder());
}

PerformanceEntry::EntryType kDroppableEntryTypes[] = {
    PerformanceEntry::kResource,
    PerformanceEntry::kLongTask,
    PerformanceEntry::kElement,
    PerformanceEntry::kEvent,
    PerformanceEntry::kLayoutShift,
    PerformanceEntry::kLargestContentfulPaint,
    PerformanceEntry::kBackForwardCacheRestoration,
};

}  // namespace

using PerformanceObserverVector = HeapVector<Member<PerformanceObserver>>;

constexpr size_t kDefaultResourceTimingBufferSize = 250;
constexpr size_t kDefaultEventTimingBufferSize = 150;
constexpr size_t kDefaultElementTimingBufferSize = 150;
constexpr size_t kDefaultLayoutShiftBufferSize = 150;
constexpr size_t kDefaultLargestContenfulPaintSize = 150;
constexpr size_t kDefaultLongTaskBufferSize = 200;
constexpr size_t kDefaultBackForwardCacheRestorationBufferSize = 200;

Performance::Performance(
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ExecutionContext* context)
    : resource_timing_buffer_size_limit_(kDefaultResourceTimingBufferSize),
      back_forward_cache_restoration_buffer_size_limit_(
          kDefaultBackForwardCacheRestorationBufferSize),
      event_timing_buffer_max_size_(kDefaultEventTimingBufferSize),
      element_timing_buffer_max_size_(kDefaultElementTimingBufferSize),
      user_timing_(nullptr),
      time_origin_(time_origin),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      cross_origin_isolated_capability_(cross_origin_isolated_capability),
      observer_filter_options_(PerformanceEntry::kInvalid),
      task_runner_(std::move(task_runner)),
      deliver_observations_timer_(task_runner_,
                                  this,
                                  &Performance::DeliverObservationsTimerFired),
      resource_timing_buffer_full_timer_(
          task_runner_,
          this,
          &Performance::FireResourceTimingBufferFull) {
  // |context| may be null in tests.
  if (context) {
    background_tracing_helper_ =
        MakeGarbageCollected<BackgroundTracingHelper>(context);
  }
  // Initialize the map of dropped entry types only with those which could be
  // dropped (saves some unnecessary 0s).
  for (const auto type : kDroppableEntryTypes) {
    dropped_entries_count_map_.insert(type, 0);
  }
}

Performance::~Performance() = default;

const AtomicString& Performance::InterfaceName() const {
  return event_target_names::kPerformance;
}

PerformanceTiming* Performance::timing() const {
  return nullptr;
}

PerformanceNavigation* Performance::navigation() const {
  return nullptr;
}

MemoryInfo* Performance::memory(ScriptState*) const {
  return nullptr;
}

EventCounts* Performance::eventCounts() {
  return nullptr;
}

ScriptPromise Performance::measureUserAgentSpecificMemory(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  return MeasureMemoryController::StartMeasurement(script_state,
                                                   exception_state);
}

DOMHighResTimeStamp Performance::timeOrigin() const {
  DCHECK(!time_origin_.is_null());
  return ClampTimeResolution(time_origin_ - base::TimeTicks::UnixEpoch(),
                             cross_origin_isolated_capability_);
}

PerformanceEntryVector Performance::getEntries() {
  PerformanceEntryVector entries;

  entries.AppendVector(resource_timing_buffer_);
  if (first_input_timing_)
    entries.push_back(first_input_timing_);
  if (!navigation_timing_)
    navigation_timing_ = CreateNavigationTimingInstance();
  // This extra checking is needed when WorkerPerformance
  // calls this method.
  if (navigation_timing_)
    entries.push_back(navigation_timing_);

  if (user_timing_) {
    entries.AppendVector(user_timing_->GetMarks());
    entries.AppendVector(user_timing_->GetMeasures());
  }

  if (first_paint_timing_)
    entries.push_back(first_paint_timing_);
  if (first_contentful_paint_timing_)
    entries.push_back(first_contentful_paint_timing_);

  if (RuntimeEnabledFeatures::NavigationIdEnabled())
    entries.AppendVector(back_forward_cache_restoration_buffer_);

  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::StartTimeCompareLessThan);
  return entries;
}

PerformanceEntryVector Performance::getBufferedEntriesByType(
    const AtomicString& entry_type) {
  PerformanceEntry::EntryType type =
      PerformanceEntry::ToEntryTypeEnum(entry_type);
  return getEntriesByTypeInternal(type);
}

PerformanceEntryVector Performance::getEntriesByType(
    const AtomicString& entry_type) {
  PerformanceEntry::EntryType type =
      PerformanceEntry::ToEntryTypeEnum(entry_type);
  if (!PerformanceEntry::IsValidTimelineEntryType(type)) {
    PerformanceEntryVector empty_entries;
    String message = "Deprecated API for given entry type.";
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning, message));
    return empty_entries;
  }
  return getEntriesByTypeInternal(type);
}

PerformanceEntryVector Performance::getEntriesByTypeInternal(
    PerformanceEntry::EntryType type) {
  PerformanceEntryVector entries;
  switch (type) {
    case PerformanceEntry::kResource:
      UseCounter::Count(GetExecutionContext(), WebFeature::kResourceTiming);
      for (const auto& resource : resource_timing_buffer_)
        entries.push_back(resource);
      break;
    case PerformanceEntry::kElement:
      for (const auto& element : element_timing_buffer_)
        entries.push_back(element);
      break;
    case PerformanceEntry::kEvent:
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kEventTimingExplicitlyRequested);
      for (const auto& event : event_timing_buffer_)
        entries.push_back(event);
      break;
    case PerformanceEntry::kFirstInput:
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kEventTimingExplicitlyRequested);
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kEventTimingFirstInputExplicitlyRequested);
      if (first_input_timing_)
        entries.push_back(first_input_timing_);
      break;
    case PerformanceEntry::kNavigation:
      UseCounter::Count(GetExecutionContext(), WebFeature::kNavigationTimingL2);
      if (!navigation_timing_)
        navigation_timing_ = CreateNavigationTimingInstance();
      if (navigation_timing_)
        entries.push_back(navigation_timing_);
      break;
    case PerformanceEntry::kMark:
      if (user_timing_)
        entries.AppendVector(user_timing_->GetMarks());
      break;
    case PerformanceEntry::kMeasure:
      if (user_timing_)
        entries.AppendVector(user_timing_->GetMeasures());
      break;
    case PerformanceEntry::kPaint:
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kPaintTimingRequested);
      if (first_paint_timing_)
        entries.push_back(first_paint_timing_);
      if (first_contentful_paint_timing_)
        entries.push_back(first_contentful_paint_timing_);
      break;
    case PerformanceEntry::kLongTask:
      for (const auto& entry : longtask_buffer_)
        entries.push_back(entry);
      break;
    // TaskAttribution entries are only associated to longtask entries.
    case PerformanceEntry::kTaskAttribution:
      break;
    case PerformanceEntry::kLayoutShift:
      for (const auto& layout_shift : layout_shift_buffer_)
        entries.push_back(layout_shift);
      break;
    case PerformanceEntry::kLargestContentfulPaint:
      entries.AppendVector(largest_contentful_paint_buffer_);
      break;
    case PerformanceEntry::kVisibilityState:
      entries.AppendVector(visibility_state_buffer_);
      break;
    case PerformanceEntry::kBackForwardCacheRestoration:
      if (RuntimeEnabledFeatures::NavigationIdEnabled())
        entries.AppendVector(back_forward_cache_restoration_buffer_);
      break;
    case PerformanceEntry::kInvalid:
      break;
  }

  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::StartTimeCompareLessThan);
  return entries;
}

PerformanceEntryVector Performance::getEntriesByName(
    const AtomicString& name,
    const AtomicString& entry_type) {
  PerformanceEntryVector entries;
  PerformanceEntry::EntryType type =
      PerformanceEntry::ToEntryTypeEnum(entry_type);

  if (!entry_type.IsNull() &&
      !PerformanceEntry::IsValidTimelineEntryType(type)) {
    String message = "Deprecated API for given entry type.";
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning, message));
    return entries;
  }

  if (entry_type.IsNull() || type == PerformanceEntry::kResource) {
    for (const auto& resource : resource_timing_buffer_) {
      if (resource->name() == name)
        entries.push_back(resource);
    }
  }

  if (entry_type.IsNull() || type == PerformanceEntry::kFirstInput) {
    if (first_input_timing_ && first_input_timing_->name() == name)
      entries.push_back(first_input_timing_);
  }
  if (type == PerformanceEntry::kFirstInput) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingExplicitlyRequested);
  }

  if (entry_type.IsNull() || type == PerformanceEntry::kNavigation) {
    if (!navigation_timing_)
      navigation_timing_ = CreateNavigationTimingInstance();
    if (navigation_timing_ && navigation_timing_->name() == name)
      entries.push_back(navigation_timing_);
  }

  if (user_timing_) {
    if (entry_type.IsNull() || type == PerformanceEntry::kMark)
      entries.AppendVector(user_timing_->GetMarks(name));
    if (entry_type.IsNull() || type == PerformanceEntry::kMeasure)
      entries.AppendVector(user_timing_->GetMeasures(name));
  }

  if (entry_type.IsNull() || type == PerformanceEntry::kPaint) {
    if (first_paint_timing_ && first_paint_timing_->name() == name)
      entries.push_back(first_paint_timing_);
    if (first_contentful_paint_timing_ &&
        first_contentful_paint_timing_->name() == name)
      entries.push_back(first_contentful_paint_timing_);
  }

  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::StartTimeCompareLessThan);
  return entries;
}

void Performance::clearResourceTimings() {
  resource_timing_buffer_.clear();
}

void Performance::setResourceTimingBufferSize(unsigned size) {
  resource_timing_buffer_size_limit_ = size;
}

void Performance::setBackForwardCacheRestorationBufferSizeForTest(
    unsigned size) {
  back_forward_cache_restoration_buffer_size_limit_ = size;
}

bool Performance::ShouldReportResponseStatus(
    const ResourceResponse& response,
    const SecurityOrigin& initiator_security_origin,
    const network::mojom::RequestMode request_mode) {
  if (request_mode != network::mojom::RequestMode::kNavigate) {
    return response.IsCorsSameOrigin();
  }
  scoped_refptr<const SecurityOrigin> response_origin =
      SecurityOrigin::Create(response.ResponseUrl());
  bool is_same_origin =
      response_origin->IsSameOriginWith(&initiator_security_origin);
  return is_same_origin;
}

void Performance::GenerateAndAddResourceTiming(
    const ResourceTimingInfo& info,
    const AtomicString& initiator_type) {
  ExecutionContext* context = GetExecutionContext();
  const SecurityOrigin* security_origin = GetSecurityOrigin(context);
  if (!security_origin)
    return;
  AddResourceTiming(
      GenerateResourceTiming(*security_origin, info, *context),
      !initiator_type.IsNull() ? initiator_type : info.InitiatorType(),
      context);
}

// Please keep this function in sync with ObjectNavigationFallbackBodyLoader's
// GenerateResourceTiming() helper.
mojom::blink::ResourceTimingInfoPtr Performance::GenerateResourceTiming(
    const SecurityOrigin& destination_origin,
    const ResourceTimingInfo& info,
    ExecutionContext& context_for_use_counter) {
  // TODO(dcheng): It would be nicer if the performance entries simply held this
  // data internally, rather than requiring it be marshalled back and forth.
  const ResourceResponse& final_response = info.FinalResponse();
  mojom::blink::ResourceTimingInfoPtr result =
      mojom::blink::ResourceTimingInfo::New();
  result->name = info.InitialURL().GetString();
  result->start_time = info.InitialTime();
  result->alpn_negotiated_protocol =
      final_response.AlpnNegotiatedProtocol().IsNull()
          ? g_empty_string
          : final_response.AlpnNegotiatedProtocol();
  result->connection_info = final_response.ConnectionInfoString().IsNull()
                                ? g_empty_string
                                : final_response.ConnectionInfoString();
  result->timing = final_response.GetResourceLoadTiming()
                       ? final_response.GetResourceLoadTiming()->ToMojo()
                       : nullptr;
  result->response_end = info.LoadResponseEnd();
  result->context_type = info.ContextType();
  result->request_destination = info.RequestDestination();

  result->allow_timing_details = final_response.TimingAllowPassed();

  const Vector<ResourceResponse>& redirect_chain = info.RedirectChain();
  if (!redirect_chain.IsEmpty()) {
    result->allow_redirect_details = result->allow_timing_details;

    // TODO(https://crbug.com/817691): is |last_chained_timing| being null a bug
    // or is this if statement reasonable?
    if (ResourceLoadTiming* last_chained_timing =
            redirect_chain.back().GetResourceLoadTiming()) {
      result->last_redirect_end_time = last_chained_timing->ReceiveHeadersEnd();
    } else {
      result->allow_redirect_details = false;
      result->last_redirect_end_time = base::TimeTicks();
    }
  } else {
    result->allow_redirect_details = false;
    result->last_redirect_end_time = base::TimeTicks();
  }

  result->cache_state = info.CacheState();
  result->encoded_body_size = final_response.EncodedBodyLength();
  result->decoded_body_size = final_response.DecodedBodyLength();
  result->did_reuse_connection = final_response.ConnectionReused();
  // Use SecurityOrigin::Create to handle cases like blob:https://.
  result->is_secure_transport = base::Contains(
      url::GetSecureSchemes(),
      SecurityOrigin::Create(final_response.ResponseUrl())->Protocol().Ascii());
  result->allow_negative_values = info.NegativeAllowed();

  if (result->allow_timing_details) {
    result->server_timing =
        PerformanceServerTiming::ParseServerTimingToMojo(info);
  }
  if (!result->server_timing.IsEmpty()) {
    UseCounter::Count(&context_for_use_counter,
                      WebFeature::kPerformanceServerTiming);
  }

  result->render_blocking_status = info.RenderBlockingStatus();
  if (ShouldReportResponseStatus(final_response, destination_origin,
                                 info.RequestMode())) {
    result->response_status = final_response.HttpStatusCode();
  }

  return result;
}

void Performance::AddResourceTiming(mojom::blink::ResourceTimingInfoPtr info,
                                    const AtomicString& initiator_type,
                                    ExecutionContext* context) {
  auto* entry = MakeGarbageCollected<PerformanceResourceTiming>(
      *info, time_origin_, cross_origin_isolated_capability_, initiator_type,
      context);
  NotifyObserversOfEntry(*entry);
  // https://w3c.github.io/resource-timing/#dfn-add-a-performanceresourcetiming-entry
  if (CanAddResourceTimingEntry() &&
      !resource_timing_buffer_full_event_pending_) {
    resource_timing_buffer_.push_back(entry);
    return;
  }
  // The Resource Timing entries have a special processing model in which there
  // is a secondary buffer but getting those entries requires handling the
  // buffer full event, and the PerformanceObserver with buffered flag only
  // receives the entries from the primary buffer, so it's ok to increase
  // the dropped entries count here.
  ++(dropped_entries_count_map_.find(PerformanceEntry::kResource)->value);
  if (!resource_timing_buffer_full_event_pending_) {
    resource_timing_buffer_full_event_pending_ = true;
    resource_timing_buffer_full_timer_.StartOneShot(base::TimeDelta(),
                                                    FROM_HERE);
  }
  resource_timing_secondary_buffer_.push_back(entry);
}

void Performance::AddResourceTimingWithUnparsedServerTiming(
    mojom::blink::ResourceTimingInfoPtr info,
    const String& server_timing_value,
    const AtomicString& initiator_type,
    ExecutionContext* context) {
  if (info->allow_timing_details) {
    info->server_timing =
        PerformanceServerTiming::ParseServerTimingFromHeaderValueToMojo(
            server_timing_value);
  }
  AddResourceTiming(std::move(info), initiator_type, context);
}

// Called after loadEventEnd happens.
void Performance::NotifyNavigationTimingToObservers() {
  if (!navigation_timing_)
    navigation_timing_ = CreateNavigationTimingInstance();
  if (navigation_timing_)
    NotifyObserversOfEntry(*navigation_timing_);
}

bool Performance::IsElementTimingBufferFull() const {
  return element_timing_buffer_.size() >= element_timing_buffer_max_size_;
}

bool Performance::IsEventTimingBufferFull() const {
  return event_timing_buffer_.size() >= event_timing_buffer_max_size_;
}

void Performance::CopySecondaryBuffer() {
  // https://w3c.github.io/resource-timing/#dfn-copy-secondary-buffer
  while (!resource_timing_secondary_buffer_.empty() &&
         CanAddResourceTimingEntry()) {
    PerformanceEntry* entry = resource_timing_secondary_buffer_.front();
    DCHECK(entry);
    resource_timing_secondary_buffer_.pop_front();
    resource_timing_buffer_.push_back(entry);
  }
}

void Performance::FireResourceTimingBufferFull(TimerBase*) {
  // https://w3c.github.io/resource-timing/#dfn-fire-a-buffer-full-event
  while (!resource_timing_secondary_buffer_.empty()) {
    int excess_entries_before = resource_timing_secondary_buffer_.size();
    if (!CanAddResourceTimingEntry()) {
      DispatchEvent(
          *Event::Create(event_type_names::kResourcetimingbufferfull));
    }
    CopySecondaryBuffer();
    int excess_entries_after = resource_timing_secondary_buffer_.size();
    if (excess_entries_after >= excess_entries_before) {
      resource_timing_secondary_buffer_.clear();
      break;
    }
  }
  resource_timing_buffer_full_event_pending_ = false;
}

void Performance::AddElementTimingBuffer(PerformanceElementTiming& entry) {
  if (!IsElementTimingBufferFull()) {
    element_timing_buffer_.push_back(&entry);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kElement)->value);
  }
}

void Performance::AddEventTimingBuffer(PerformanceEventTiming& entry) {
  if (!IsEventTimingBufferFull()) {
    event_timing_buffer_.push_back(&entry);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kEvent)->value);
  }
}

void Performance::AddLayoutShiftBuffer(LayoutShift& entry) {
  probe::PerformanceEntryAdded(GetExecutionContext(), &entry);
  if (layout_shift_buffer_.size() < kDefaultLayoutShiftBufferSize) {
    layout_shift_buffer_.push_back(&entry);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kLayoutShift)->value);
  }
}

void Performance::AddLargestContentfulPaint(LargestContentfulPaint* entry) {
  probe::PerformanceEntryAdded(GetExecutionContext(), entry);
  if (largest_contentful_paint_buffer_.size() <
      kDefaultLargestContenfulPaintSize) {
    largest_contentful_paint_buffer_.push_back(entry);
  } else {
    ++(dropped_entries_count_map_
           .find(PerformanceEntry::kLargestContentfulPaint)
           ->value);
  }
}

void Performance::AddFirstPaintTiming(base::TimeTicks start_time) {
  AddPaintTiming(PerformancePaintTiming::PaintType::kFirstPaint, start_time);
}

void Performance::AddFirstContentfulPaintTiming(base::TimeTicks start_time) {
  AddPaintTiming(PerformancePaintTiming::PaintType::kFirstContentfulPaint,
                 start_time);
}

void Performance::AddPaintTiming(PerformancePaintTiming::PaintType type,
                                 base::TimeTicks start_time) {
  PerformanceEntry* entry = MakeGarbageCollected<PerformancePaintTiming>(
      type, MonotonicTimeToDOMHighResTimeStamp(start_time),
      PerformanceEntry::GetNavigationId(GetExecutionContext()));
  // Always buffer First Paint & First Contentful Paint.
  if (type == PerformancePaintTiming::PaintType::kFirstPaint)
    first_paint_timing_ = entry;
  else if (type == PerformancePaintTiming::PaintType::kFirstContentfulPaint)
    first_contentful_paint_timing_ = entry;
  NotifyObserversOfEntry(*entry);
}

bool Performance::CanAddResourceTimingEntry() {
  // https://w3c.github.io/resource-timing/#dfn-can-add-resource-timing-entry
  return resource_timing_buffer_.size() < resource_timing_buffer_size_limit_;
}

void Performance::AddLongTaskTiming(base::TimeTicks start_time,
                                    base::TimeTicks end_time,
                                    const AtomicString& name,
                                    const AtomicString& container_type,
                                    const AtomicString& container_src,
                                    const AtomicString& container_id,
                                    const AtomicString& container_name) {
  double dom_high_res_start_time =
      MonotonicTimeToDOMHighResTimeStamp(start_time);

  ExecutionContext* execution_context = GetExecutionContext();
  auto* entry = MakeGarbageCollected<PerformanceLongTaskTiming>(
      dom_high_res_start_time,
      // Convert the delta between start and end times to an int to reduce the
      // granularity of the duration to 1 ms.
      static_cast<int>(MonotonicTimeToDOMHighResTimeStamp(end_time) -
                       dom_high_res_start_time),
      name, container_type, container_src, container_id, container_name,
      PerformanceEntry::GetNavigationId(execution_context));
  if (longtask_buffer_.size() < kDefaultLongTaskBufferSize) {
    longtask_buffer_.push_back(entry);
  } else {
    ++(dropped_entries_count_map_.find(PerformanceEntry::kLongTask)->value);
    UseCounter::Count(execution_context, WebFeature::kLongTaskBufferFull);
  }
  if ((++long_task_counter_ % kLongTaskUkmSampleInterval) == 0) {
    RecordLongTaskUkm(execution_context,
                      base::Milliseconds(dom_high_res_start_time),
                      end_time - start_time);
  }
  NotifyObserversOfEntry(*entry);
}

void Performance::AddBackForwardCacheRestoration(
    base::TimeTicks start_time,
    base::TimeTicks pageshow_start_time,
    base::TimeTicks pageshow_end_time) {
  auto* entry = MakeGarbageCollected<BackForwardCacheRestoration>(
      MonotonicTimeToDOMHighResTimeStamp(start_time),
      MonotonicTimeToDOMHighResTimeStamp(pageshow_start_time),
      MonotonicTimeToDOMHighResTimeStamp(pageshow_end_time),
      PerformanceEntry::GetNavigationId(GetExecutionContext()));
  if (back_forward_cache_restoration_buffer_.size() <
      back_forward_cache_restoration_buffer_size_limit_) {
    back_forward_cache_restoration_buffer_.push_back(entry);
  } else {
    ++(dropped_entries_count_map_
           .find(PerformanceEntry::kBackForwardCacheRestoration)
           ->value);
  }
  NotifyObserversOfEntry(*entry);
}

UserTiming& Performance::GetUserTiming() {
  if (!user_timing_)
    user_timing_ = MakeGarbageCollected<UserTiming>(*this);
  return *user_timing_;
}

PerformanceMark* Performance::mark(ScriptState* script_state,
                                   const AtomicString& mark_name,
                                   PerformanceMarkOptions* mark_options,
                                   ExceptionState& exception_state) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const AtomicString, mark_fully_loaded,
                                  ("mark_fully_loaded"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const AtomicString, mark_fully_visible,
                                  ("mark_fully_visible"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const AtomicString, mark_interactive,
                                  ("mark_interactive"));
  if (mark_options &&
      (mark_options->hasStartTime() || mark_options->hasDetail())) {
    UseCounter::Count(GetExecutionContext(), WebFeature::kUserTimingL3);
  }
  PerformanceMark* performance_mark = PerformanceMark::Create(
      script_state, mark_name, mark_options, exception_state);
  if (performance_mark) {
    background_tracing_helper_->MaybeEmitBackgroundTracingPerformanceMarkEvent(
        *performance_mark);
    GetUserTiming().AddMarkToPerformanceTimeline(*performance_mark);
    if (mark_name == mark_fully_loaded) {
      if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
        window->GetFrame()
            ->Loader()
            .GetDocumentLoader()
            ->GetTiming()
            .SetUserTimingMarkFullyLoaded(
                base::Milliseconds(performance_mark->startTime()));
      }
    } else if (mark_name == mark_fully_visible) {
      if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
        window->GetFrame()
            ->Loader()
            .GetDocumentLoader()
            ->GetTiming()
            .SetUserTimingMarkFullyVisible(
                base::Milliseconds(performance_mark->startTime()));
      }
    } else if (mark_name == mark_interactive) {
      if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
        window->GetFrame()
            ->Loader()
            .GetDocumentLoader()
            ->GetTiming()
            .SetUserTimingMarkInteractive(
                base::Milliseconds(performance_mark->startTime()));
      }
    }
    NotifyObserversOfEntry(*performance_mark);
  }
  return performance_mark;
}

void Performance::clearMarks(const AtomicString& mark_name) {
  GetUserTiming().ClearMarks(mark_name);
}

PerformanceMeasure* Performance::measure(ScriptState* script_state,
                                         const AtomicString& measure_name,
                                         ExceptionState& exception_state) {
  // When |startOrOptions| is not provided, it's assumed to be an empty
  // dictionary.
  return MeasureInternal(script_state, measure_name, nullptr, absl::nullopt,
                         exception_state);
}

PerformanceMeasure* Performance::measure(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const V8UnionPerformanceMeasureOptionsOrString* start_or_options,
    ExceptionState& exception_state) {
  return MeasureInternal(script_state, measure_name, start_or_options,
                         absl::nullopt, exception_state);
}

PerformanceMeasure* Performance::measure(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const V8UnionPerformanceMeasureOptionsOrString* start_or_options,
    const String& end,
    ExceptionState& exception_state) {
  return MeasureInternal(script_state, measure_name, start_or_options,
                         absl::optional<String>(end), exception_state);
}

// |MeasureInternal| exists to unify the arguments from different
// `performance.measure()` overloads into a consistent form, then delegate to
// |MeasureWithDetail|.
//
// |start_or_options| is either a String or a dictionary of options. When it's
// a String, it represents a starting performance mark. When it's a dictionary,
// the allowed fields are 'start', 'duration', 'end' and 'detail'. However,
// there are some combinations of fields and parameters which must raise
// errors. Specifically, the spec (https://https://w3c.github.io/user-timing/)
// requires errors to thrown in the following cases:
//  - If |start_or_options| is a dictionary and 'end_mark' is passed.
//  - If an options dictionary contains neither a 'start' nor an 'end' field.
//  - If an options dictionary contains all of 'start', 'duration' and 'end'.
//
// |end_mark| will be absl::nullopt unless the `performance.measure()` overload
// specified an end mark.
PerformanceMeasure* Performance::MeasureInternal(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const V8UnionPerformanceMeasureOptionsOrString* start_or_options,
    absl::optional<String> end_mark,
    ExceptionState& exception_state) {
  // An empty option is treated with no difference as null, undefined.
  if (start_or_options && start_or_options->IsPerformanceMeasureOptions() &&
      !IsMeasureOptionsEmpty(
          *start_or_options->GetAsPerformanceMeasureOptions())) {
    UseCounter::Count(GetExecutionContext(), WebFeature::kUserTimingL3);
    // measure("name", { start, end }, *)
    if (end_mark) {
      exception_state.ThrowTypeError(
          "If a non-empty PerformanceMeasureOptions object was passed, "
          "|end_mark| must not be passed.");
      return nullptr;
    }
    const PerformanceMeasureOptions* options =
        start_or_options->GetAsPerformanceMeasureOptions();
    if (!options->hasStart() && !options->hasEnd()) {
      exception_state.ThrowTypeError(
          "If a non-empty PerformanceMeasureOptions object was passed, at "
          "least one of its 'start' or 'end' properties must be present.");
      return nullptr;
    }

    if (options->hasStart() && options->hasDuration() && options->hasEnd()) {
      exception_state.ThrowTypeError(
          "If a non-empty PerformanceMeasureOptions object was passed, it "
          "must not have all of its 'start', 'duration', and 'end' "
          "properties defined");
      return nullptr;
    }

    V8UnionDoubleOrString* start = options->getStartOr(nullptr);
    absl::optional<double> duration;
    if (options->hasDuration()) {
      duration = options->duration();
    }
    V8UnionDoubleOrString* end = options->getEndOr(nullptr);

    return MeasureWithDetail(
        script_state, measure_name, start, duration, end,
        options->hasDetail() ? options->detail() : ScriptValue(),
        exception_state);
  }

  // measure("name", "mark1", *)
  V8UnionDoubleOrString* start = nullptr;
  if (start_or_options && start_or_options->IsString()) {
    start = MakeGarbageCollected<V8UnionDoubleOrString>(
        start_or_options->GetAsString());
  }
  // We let |end_mark| behave the same whether it's empty, undefined or null
  // in JS, as long as |end_mark| is null in C++.
  V8UnionDoubleOrString* end = nullptr;
  if (end_mark) {
    end = MakeGarbageCollected<V8UnionDoubleOrString>(*end_mark);
  }
  return MeasureWithDetail(script_state, measure_name, start,
                           /* duration = */ absl::nullopt, end,
                           ScriptValue::CreateNull(script_state->GetIsolate()),
                           exception_state);
}

PerformanceMeasure* Performance::MeasureWithDetail(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const V8UnionDoubleOrString* start,
    const absl::optional<double>& duration,
    const V8UnionDoubleOrString* end,
    const ScriptValue& detail,
    ExceptionState& exception_state) {
  PerformanceMeasure* performance_measure =
      GetUserTiming().Measure(script_state, measure_name, start, duration, end,
                              detail, exception_state);
  if (performance_measure)
    NotifyObserversOfEntry(*performance_measure);
  return performance_measure;
}

void Performance::clearMeasures(const AtomicString& measure_name) {
  GetUserTiming().ClearMeasures(measure_name);
}

void Performance::RegisterPerformanceObserver(PerformanceObserver& observer) {
  observer_filter_options_ |= observer.FilterOptions();
  observers_.insert(&observer);
}

void Performance::UnregisterPerformanceObserver(
    PerformanceObserver& old_observer) {
  observers_.erase(&old_observer);
  UpdatePerformanceObserverFilterOptions();
}

void Performance::UpdatePerformanceObserverFilterOptions() {
  observer_filter_options_ = PerformanceEntry::kInvalid;
  for (const auto& observer : observers_) {
    observer_filter_options_ |= observer->FilterOptions();
  }
}

void Performance::NotifyObserversOfEntry(PerformanceEntry& entry) const {
  bool observer_found = false;
  for (auto& observer : observers_) {
    if (observer->FilterOptions() & entry.EntryTypeEnum() &&
        observer->CanObserve(entry)) {
      observer->EnqueuePerformanceEntry(entry);
      observer_found = true;
    }
  }
  if (observer_found && entry.EntryTypeEnum() == PerformanceEntry::kPaint)
    UseCounter::Count(GetExecutionContext(), WebFeature::kPaintTimingObserved);
}

bool Performance::HasObserverFor(
    PerformanceEntry::EntryType filter_type) const {
  return observer_filter_options_ & filter_type;
}

void Performance::ActivateObserver(PerformanceObserver& observer) {
  if (active_observers_.IsEmpty())
    deliver_observations_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  if (suspended_observers_.Contains(&observer))
    suspended_observers_.erase(&observer);
  active_observers_.insert(&observer);
}

void Performance::SuspendObserver(PerformanceObserver& observer) {
  DCHECK(!suspended_observers_.Contains(&observer));
  if (!active_observers_.Contains(&observer))
    return;
  active_observers_.erase(&observer);
  suspended_observers_.insert(&observer);
}

void Performance::DeliverObservationsTimerFired(TimerBase*) {
  decltype(active_observers_) observers;
  active_observers_.Swap(observers);
  for (const auto& observer : observers) {
    observer->Deliver(observer->RequiresDroppedEntries()
                          ? absl::optional<int>(GetDroppedEntriesForTypes(
                                observer->FilterOptions()))
                          : absl::nullopt);
  }
}

int Performance::GetDroppedEntriesForTypes(PerformanceEntryTypeMask types) {
  int dropped_count = 0;
  for (const auto type : kDroppableEntryTypes) {
    if (types & type)
      dropped_count += dropped_entries_count_map_.at(type);
  }
  return dropped_count;
}

// static
DOMHighResTimeStamp Performance::ClampTimeResolution(
    base::TimeDelta time,
    bool cross_origin_isolated_capability) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(TimeClamper, clamper, ());
  return clamper.ClampTimeResolution(time, cross_origin_isolated_capability)
      .InMillisecondsF();
}

// static
DOMHighResTimeStamp Performance::MonotonicTimeToDOMHighResTimeStamp(
    base::TimeTicks time_origin,
    base::TimeTicks monotonic_time,
    bool allow_negative_value,
    bool cross_origin_isolated_capability) {
  // Avoid exposing raw platform timestamps.
  if (monotonic_time.is_null() || time_origin.is_null())
    return 0.0;

  DOMHighResTimeStamp clamped_time =
      ClampTimeResolution(monotonic_time.since_origin(),
                          cross_origin_isolated_capability) -
      ClampTimeResolution(time_origin.since_origin(),
                          cross_origin_isolated_capability);
  if (clamped_time < 0 && !allow_negative_value)
    return 0.0;
  return clamped_time;
}

// static
base::TimeDelta Performance::MonotonicTimeToTimeDelta(
    base::TimeTicks time_origin,
    base::TimeTicks monotonic_time,
    bool allow_negative_value,
    bool cross_origin_isolated_capability) {
  return base::Milliseconds(MonotonicTimeToDOMHighResTimeStamp(
      time_origin, monotonic_time, allow_negative_value,
      cross_origin_isolated_capability));
}

DOMHighResTimeStamp Performance::MonotonicTimeToDOMHighResTimeStamp(
    base::TimeTicks monotonic_time) const {
  return MonotonicTimeToDOMHighResTimeStamp(time_origin_, monotonic_time,
                                            false /* allow_negative_value */,
                                            cross_origin_isolated_capability_);
}

base::TimeDelta Performance::MonotonicTimeToTimeDelta(
    base::TimeTicks monotonic_time) const {
  return MonotonicTimeToTimeDelta(time_origin_, monotonic_time,
                                  false /* allow_negative_value */,
                                  cross_origin_isolated_capability_);
}

DOMHighResTimeStamp Performance::now() const {
  return MonotonicTimeToDOMHighResTimeStamp(tick_clock_->NowTicks());
}

// static
bool Performance::CanExposeNode(Node* node) {
  if (!node || !node->isConnected() || node->IsInShadowTree())
    return false;

  // Do not expose |node| when the document is not 'fully active'.
  const Document& document = node->GetDocument();
  if (!document.IsActive() || !document.GetFrame())
    return false;

  return true;
}

ScriptValue Performance::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  BuildJSONValue(result);
  return result.GetScriptValue();
}

void Performance::BuildJSONValue(V8ObjectBuilder& builder) const {
  builder.AddNumber("timeOrigin", timeOrigin());
  // |memory| is not part of the spec, omitted.
}

void Performance::Trace(Visitor* visitor) const {
  visitor->Trace(resource_timing_buffer_);
  visitor->Trace(resource_timing_secondary_buffer_);
  visitor->Trace(element_timing_buffer_);
  visitor->Trace(event_timing_buffer_);
  visitor->Trace(layout_shift_buffer_);
  visitor->Trace(largest_contentful_paint_buffer_);
  visitor->Trace(longtask_buffer_);
  visitor->Trace(visibility_state_buffer_);
  visitor->Trace(back_forward_cache_restoration_buffer_);
  visitor->Trace(navigation_timing_);
  visitor->Trace(user_timing_);
  visitor->Trace(first_paint_timing_);
  visitor->Trace(first_contentful_paint_timing_);
  visitor->Trace(first_input_timing_);
  visitor->Trace(observers_);
  visitor->Trace(active_observers_);
  visitor->Trace(suspended_observers_);
  visitor->Trace(deliver_observations_timer_);
  visitor->Trace(resource_timing_buffer_full_timer_);
  visitor->Trace(background_tracing_helper_);
  EventTargetWithInlineData::Trace(visitor);
}

void Performance::SetTickClockForTesting(const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void Performance::ResetTimeOriginForTesting(base::TimeTicks time_origin) {
  time_origin_ = time_origin;
}

}  // namespace blink
