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
#include "third_party/blink/renderer/bindings/core/v8/double_or_performance_mark_options.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_double_or_performance_measure_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_long_task_timing.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/core/timing/performance_measure.h"
#include "third_party/blink/renderer/core/timing/performance_measure_options.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"
#include "third_party/blink/renderer/core/timing/performance_user_timing.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

namespace {

const SecurityOrigin* GetSecurityOrigin(ExecutionContext* context) {
  if (context)
    return context->GetSecurityOrigin();
  return nullptr;
}

// When a Performance object is first created, use the current system time
// to calculate what the Unix time would be at the time the monotonic clock time
// was zero, assuming no manual changes to the system clock. This can be
// calculated as current_unix_time - current_monotonic_time.
DOMHighResTimeStamp GetUnixAtZeroMonotonic() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      DOMHighResTimeStamp, unix_at_zero_monotonic,
      {ConvertSecondsToDOMHighResTimeStamp(CurrentTime() -
                                           CurrentTimeTicksInSeconds())});
  return unix_at_zero_monotonic;
}

}  // namespace

using PerformanceObserverVector = HeapVector<Member<PerformanceObserver>>;

static const size_t kDefaultResourceTimingBufferSize = 250;
constexpr size_t kDefaultEventTimingBufferSize = 150;

Performance::Performance(
    TimeTicks time_origin,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : resource_timing_buffer_size_(kDefaultResourceTimingBufferSize),
      event_timing_buffer_max_size_(kDefaultEventTimingBufferSize),
      user_timing_(nullptr),
      time_origin_(time_origin),
      observer_filter_options_(PerformanceEntry::kInvalid),
      deliver_observations_timer_(std::move(task_runner),
                                  this,
                                  &Performance::DeliverObservationsTimerFired) {
}

Performance::~Performance() = default;

const AtomicString& Performance::InterfaceName() const {
  return EventTargetNames::Performance;
}

PerformanceTiming* Performance::timing() const {
  return nullptr;
}

PerformanceNavigation* Performance::navigation() const {
  return nullptr;
}

MemoryInfo* Performance::memory() const {
  return nullptr;
}

bool Performance::shouldYield() const {
  return false;
}

DOMHighResTimeStamp Performance::timeOrigin() const {
  DCHECK(!time_origin_.is_null());
  return GetUnixAtZeroMonotonic() +
         ConvertTimeTicksToDOMHighResTimeStamp(time_origin_);
}

PerformanceEntryVector Performance::getEntries() {
  PerformanceEntryVector entries;

  entries.AppendVector(resource_timing_buffer_);
  entries.AppendVector(event_timing_buffer_);
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

  std::sort(entries.begin(), entries.end(),
            PerformanceEntry::StartTimeCompareLessThan);
  return entries;
}

PerformanceEntryVector Performance::getEntriesByType(
    const AtomicString& entry_type) {
  PerformanceEntryVector entries;
  PerformanceEntry::EntryType type =
      PerformanceEntry::ToEntryTypeEnum(entry_type);

  switch (type) {
    case PerformanceEntry::kResource:
      for (const auto& resource : resource_timing_buffer_)
        entries.push_back(resource);
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
      if (first_input_timing_)
        entries.push_back(first_input_timing_);
      break;
    case PerformanceEntry::kNavigation:
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
    // Unsupported for LongTask, TaskAttribution.
    // Per the spec, these entries can only be accessed via
    // Performance Observer. No separate buffer is maintained.
    case PerformanceEntry::kLongTask:
      break;
    case PerformanceEntry::kTaskAttribution:
      break;
    // TODO(npm): decide which element timing and layout jank entries are
    // accessible via the performance buffer.
    case PerformanceEntry::kElement:
    case PerformanceEntry::kLayoutJank:
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

  if (!entry_type.IsNull() && type == PerformanceEntry::kInvalid)
    return entries;

  if (entry_type.IsNull() || type == PerformanceEntry::kResource) {
    for (const auto& resource : resource_timing_buffer_) {
      if (resource->name() == name)
        entries.push_back(resource);
    }
  }

  if (entry_type.IsNull() || type == PerformanceEntry::kEvent) {
    for (const auto& event : event_timing_buffer_) {
      if (event->name() == name)
        entries.push_back(event);
    }
  }
  if (type == PerformanceEntry::kEvent) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingExplicitlyRequested);
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
  resource_timing_buffer_size_ = size;
  if (IsResourceTimingBufferFull())
    DispatchEvent(*Event::Create(EventTypeNames::resourcetimingbufferfull));
}

bool Performance::PassesTimingAllowCheck(
    const ResourceResponse& response,
    const SecurityOrigin& initiator_security_origin,
    const AtomicString& original_timing_allow_origin,
    ExecutionContext* context) {
  const KURL& response_url = response.WasFetchedViaServiceWorker()
                                 ? response.OriginalURLViaServiceWorker()
                                 : response.Url();
  scoped_refptr<const SecurityOrigin> resource_origin =
      SecurityOrigin::Create(response_url);
  if (resource_origin->IsSameSchemeHostPort(&initiator_security_origin))
    return true;

  const AtomicString& timing_allow_origin_string =
      original_timing_allow_origin.IsEmpty()
          ? response.HttpHeaderField(HTTPNames::Timing_Allow_Origin)
          : original_timing_allow_origin;
  if (timing_allow_origin_string.IsEmpty() ||
      EqualIgnoringASCIICase(timing_allow_origin_string, "null"))
    return false;

  // The condition below if only needed for use-counting purposes.
  if (timing_allow_origin_string == "*") {
    UseCounter::Count(context, WebFeature::kStarInTimingAllowOrigin);
    return true;
  }

  // TODO(yoav): Use CommaDelimitedHeaderSet instead of this one-off parsing
  // algorithm.
  const String& security_origin = initiator_security_origin.ToString();
  Vector<String> timing_allow_origins;
  timing_allow_origin_string.GetString().Split(',', timing_allow_origins);
  if (timing_allow_origins.size() > 1) {
    UseCounter::Count(context, WebFeature::kMultipleOriginsInTimingAllowOrigin);
  } else if (timing_allow_origins.size() == 1 &&
             timing_allow_origin_string != "*") {
    UseCounter::Count(context, WebFeature::kSingleOriginInTimingAllowOrigin);
  }
  for (const String& allow_origin : timing_allow_origins) {
    const String allow_origin_stripped = allow_origin.StripWhiteSpace();
    if (allow_origin_stripped == security_origin ||
        allow_origin_stripped == "*") {
      return true;
    }
  }

  return false;
}

bool Performance::AllowsTimingRedirect(
    const Vector<ResourceResponse>& redirect_chain,
    const ResourceResponse& final_response,
    const SecurityOrigin& initiator_security_origin,
    ExecutionContext* context) {
  if (!PassesTimingAllowCheck(final_response, initiator_security_origin,
                              AtomicString(), context))
    return false;

  for (const ResourceResponse& response : redirect_chain) {
    if (!PassesTimingAllowCheck(response, initiator_security_origin,
                                AtomicString(), context))
      return false;
  }

  return true;
}

void Performance::GenerateAndAddResourceTiming(
    const ResourceTimingInfo& info,
    const AtomicString& initiator_type) {
  if (IsResourceTimingBufferFull() &&
      !HasObserverFor(PerformanceEntry::kResource))
    return;

  ExecutionContext* context = GetExecutionContext();
  const SecurityOrigin* security_origin = GetSecurityOrigin(context);
  if (!security_origin)
    return;
  AddResourceTiming(
      GenerateResourceTiming(*security_origin, info, *context),
      !initiator_type.IsNull() ? initiator_type : info.InitiatorType());
}

WebResourceTimingInfo Performance::GenerateResourceTiming(
    const SecurityOrigin& destination_origin,
    const ResourceTimingInfo& info,
    ExecutionContext& context_for_use_counter) {
  // TODO(dcheng): It would be nicer if the performance entries simply held this
  // data internally, rather than requiring it be marshalled back and forth.
  const ResourceResponse& final_response = info.FinalResponse();
  WebResourceTimingInfo result;
  result.name = info.InitialURL().GetString();
  result.start_time = info.InitialTime();
  result.alpn_negotiated_protocol = final_response.AlpnNegotiatedProtocol();
  result.connection_info = final_response.ConnectionInfoString();
  result.timing = final_response.GetResourceLoadTiming();
  result.finish_time = info.LoadFinishTime();

  result.allow_timing_details = PassesTimingAllowCheck(
      final_response, destination_origin, info.OriginalTimingAllowOrigin(),
      &context_for_use_counter);

  const Vector<ResourceResponse>& redirect_chain = info.RedirectChain();
  if (!redirect_chain.IsEmpty()) {
    result.allow_redirect_details =
        AllowsTimingRedirect(redirect_chain, final_response, destination_origin,
                             &context_for_use_counter);

    // TODO(https://crbug.com/817691): is |last_chained_timing| being null a bug
    // or is this if statement reasonable?
    if (ResourceLoadTiming* last_chained_timing =
            redirect_chain.back().GetResourceLoadTiming()) {
      result.last_redirect_end_time = last_chained_timing->ReceiveHeadersEnd();
    } else {
      result.allow_redirect_details = false;
      result.last_redirect_end_time = TimeTicks();
    }
    if (!result.allow_redirect_details) {
      // TODO(https://crbug.com/817691): There was previously a DCHECK that
      // |final_timing| is non-null. However, it clearly can be null: removing
      // this check caused https://crbug.com/803811. Figure out how this can
      // happen so test coverage can be added.
      if (ResourceLoadTiming* final_timing =
              final_response.GetResourceLoadTiming()) {
        result.start_time = final_timing->RequestTime();
      }
    }
  } else {
    result.allow_redirect_details = false;
    result.last_redirect_end_time = TimeTicks();
  }

  result.transfer_size = info.TransferSize();
  result.encoded_body_size = final_response.EncodedBodyLength();
  result.decoded_body_size = final_response.DecodedBodyLength();
  result.did_reuse_connection = final_response.ConnectionReused();
  result.allow_negative_values = info.NegativeAllowed();

  if (result.allow_timing_details) {
    result.server_timing = PerformanceServerTiming::ParseServerTiming(info);
  }
  if (!result.server_timing.empty()) {
    UseCounter::Count(&context_for_use_counter,
                      WebFeature::kPerformanceServerTiming);
  }

  return result;
}

void Performance::AddResourceTiming(const WebResourceTimingInfo& info,
                                    const AtomicString& initiator_type) {
  if (IsResourceTimingBufferFull() &&
      !HasObserverFor(PerformanceEntry::kResource))
    return;

  PerformanceEntry* entry =
      PerformanceResourceTiming::Create(info, time_origin_, initiator_type);
  NotifyObserversOfEntry(*entry);
  if (!IsResourceTimingBufferFull())
    AddResourceTimingBuffer(*entry);
}

// Called after loadEventEnd happens.
void Performance::NotifyNavigationTimingToObservers() {
  if (!navigation_timing_)
    navigation_timing_ = CreateNavigationTimingInstance();
  if (navigation_timing_)
    NotifyObserversOfEntry(*navigation_timing_);
}

bool Performance::IsEventTimingBufferFull() const {
  return event_timing_buffer_.size() >= event_timing_buffer_max_size_;
}

void Performance::AddEventTimingBuffer(PerformanceEventTiming& entry) {
  event_timing_buffer_.push_back(&entry);

  if (IsEventTimingBufferFull())
    DispatchEvent(*Event::Create(EventTypeNames::eventtimingbufferfull));
}

unsigned Performance::EventTimingBufferSize() const {
  return event_timing_buffer_.size();
}

void Performance::clearEventTimings() {
  event_timing_buffer_.clear();
}

void Performance::setEventTimingBufferMaxSize(unsigned size) {
  event_timing_buffer_max_size_ = size;
  if (IsEventTimingBufferFull())
    DispatchEvent(*Event::Create(EventTypeNames::eventtimingbufferfull));
}

void Performance::AddFirstPaintTiming(TimeTicks start_time) {
  AddPaintTiming(PerformancePaintTiming::PaintType::kFirstPaint, start_time);
}

void Performance::AddFirstContentfulPaintTiming(TimeTicks start_time) {
  AddPaintTiming(PerformancePaintTiming::PaintType::kFirstContentfulPaint,
                 start_time);
}

void Performance::AddPaintTiming(PerformancePaintTiming::PaintType type,
                                 TimeTicks start_time) {
  PerformanceEntry* entry = new PerformancePaintTiming(
      type, MonotonicTimeToDOMHighResTimeStamp(start_time));
  // Always buffer First Paint & First Contentful Paint.
  if (type == PerformancePaintTiming::PaintType::kFirstPaint)
    first_paint_timing_ = entry;
  else if (type == PerformancePaintTiming::PaintType::kFirstContentfulPaint)
    first_contentful_paint_timing_ = entry;
  NotifyObserversOfEntry(*entry);
}

void Performance::AddResourceTimingBuffer(PerformanceEntry& entry) {
  resource_timing_buffer_.push_back(&entry);

  if (IsResourceTimingBufferFull())
    DispatchEvent(*Event::Create(EventTypeNames::resourcetimingbufferfull));
}

bool Performance::IsResourceTimingBufferFull() {
  return resource_timing_buffer_.size() >= resource_timing_buffer_size_;
}

void Performance::AddLongTaskTiming(
    TimeTicks start_time,
    TimeTicks end_time,
    const AtomicString& name,
    const String& frame_src,
    const String& frame_id,
    const String& frame_name,
    const SubTaskAttribution::EntriesVector& sub_task_attributions) {
  if (!HasObserverFor(PerformanceEntry::kLongTask))
    return;

  for (auto&& it : sub_task_attributions) {
    it->setHighResStartTime(
        MonotonicTimeToDOMHighResTimeStamp(it->startTime()));
    it->setHighResDuration(
        ConvertTimeDeltaToDOMHighResTimeStamp(it->duration()));
  }
  PerformanceEntry* entry = PerformanceLongTaskTiming::Create(
      MonotonicTimeToDOMHighResTimeStamp(start_time),
      MonotonicTimeToDOMHighResTimeStamp(end_time), name, frame_src, frame_id,
      frame_name, sub_task_attributions);
  NotifyObserversOfEntry(*entry);
}

PerformanceMark* Performance::mark(ScriptState* script_state,
                                   const AtomicString& mark_name,
                                   ExceptionState& exception_state) {
  DoubleOrPerformanceMarkOptions startOrOptions;
  return this->mark(script_state, mark_name, startOrOptions, exception_state);
}

PerformanceMark* Performance::mark(
    ScriptState* script_state,
    const AtomicString& mark_name,
    DoubleOrPerformanceMarkOptions& start_time_or_mark_options,
    ExceptionState& exception_state) {
  if (!RuntimeEnabledFeatures::CustomUserTimingEnabled()) {
    DCHECK(start_time_or_mark_options.IsNull());
  }

  if (!user_timing_)
    user_timing_ = UserTiming::Create(*this);

  DOMHighResTimeStamp start = 0.0;
  if (start_time_or_mark_options.IsDouble()) {
    start = start_time_or_mark_options.GetAsDouble();
  } else if (start_time_or_mark_options.IsPerformanceMarkOptions() &&
             start_time_or_mark_options.GetAsPerformanceMarkOptions()
                 .hasStartTime()) {
    start =
        start_time_or_mark_options.GetAsPerformanceMarkOptions().startTime();
  } else {
    start = now();
  }

  ScriptValue detail = ScriptValue::CreateNull(script_state);
  if (start_time_or_mark_options.IsPerformanceMarkOptions()) {
    detail = start_time_or_mark_options.GetAsPerformanceMarkOptions().detail();
  }

  // Pass in a null ScriptValue if the mark's detail doesn't exist.
  PerformanceMark* performance_mark = user_timing_->Mark(
      script_state, mark_name, start, detail, exception_state);
  if (performance_mark)
    NotifyObserversOfEntry(*performance_mark);
  return performance_mark;
}

void Performance::clearMarks(const AtomicString& mark_name) {
  if (!user_timing_)
    user_timing_ = UserTiming::Create(*this);
  user_timing_->ClearMarks(mark_name);
}

PerformanceMeasure* Performance::measure(ScriptState* script_state,
                                         const AtomicString& measure_name,
                                         ExceptionState& exception_state) {
  return measureInternal(script_state, measure_name,
                         NativeValueTraits<StringOrDouble>::NullValue(),
                         NativeValueTraits<StringOrDouble>::NullValue(),
                         ScriptValue::CreateNull(script_state),
                         exception_state);
}

PerformanceMeasure* Performance::measure(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const StringOrDoubleOrPerformanceMeasureOptions& start_or_options,
    ExceptionState& exception_state) {
  return measureInternal(script_state, measure_name, start_or_options,
                         NativeValueTraits<StringOrDouble>::NullValue(),
                         exception_state);
}

PerformanceMeasure* Performance::measure(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const StringOrDoubleOrPerformanceMeasureOptions& start_or_options,
    const StringOrDouble& end,
    ExceptionState& exception_state) {
  return measureInternal(script_state, measure_name, start_or_options, end,
                         exception_state);
}

// |start_or_options|: while in options type, the value is an object {start,
// end, detail}, and |end| must be null; while in string or double type, the
// value is start. So there are two ways we can input the start and end
// (omitting |script_state| and |measure_name|):
// 1. measure(start, end)
// 2. measure({start, end})
//
// For simplicity, the method below unifies these ways into a single form -
// measure(name, start, end, detail). The mapping between two measure methods
// (using measure_ to denote the measure after tranformation) goes as follows:
// 1. measure(start, end): measure_(start, end, null)
// 2. measure({start, end, detail}, null): measure_(start, end, detail)
// 3. measure({start, end, detail}, end): invalid
//
// When |end| is null in C++, we cannot tell whether |end| is null, undefined or
// empty in JS from StringOrDouble, so we need |end_is_empty| to help
// distinguish between (null or undefined) and empty.
PerformanceMeasure* Performance::measureInternal(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const StringOrDoubleOrPerformanceMeasureOptions& start_or_options,
    const StringOrDouble& end,
    ExceptionState& exception_state) {
  if (RuntimeEnabledFeatures::CustomUserTimingEnabled()) {
    if (start_or_options.IsPerformanceMeasureOptions()) {
      if (!end.IsNull()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kSyntaxError,
            "If a PerformanceMeasureOptions object was passed, |end| must be "
            "null.");
        return nullptr;
      }
      const PerformanceMeasureOptions& options =
          start_or_options.GetAsPerformanceMeasureOptions();
      return measureInternal(script_state, measure_name, options.startTime(),
                             options.endTime(), options.detail(),
                             exception_state);
    } else {
      StringOrDouble converted_start;
      if (start_or_options.IsDouble()) {
        converted_start =
            StringOrDouble::FromDouble(start_or_options.GetAsDouble());
      } else if (start_or_options.IsString()) {
        converted_start =
            StringOrDouble::FromString(start_or_options.GetAsString());
      } else {
        DCHECK(start_or_options.IsNull());
        converted_start = NativeValueTraits<StringOrDouble>::NullValue();
      }
      // We let |end| behaves the same whether it's empty, undefined or null in
      // JS, as long as |end| is null in C++.
      return measureInternal(script_state, measure_name, converted_start, end,
                             ScriptValue::CreateNull(script_state),
                             exception_state);
    }
  } else {
    // For consistency with UserTimingL2: the L2 API took |start| as a string,
    // so any object passed in became a string '[object, object]', null became
    // string 'null'.
    StringOrDouble converted_start;
    if (start_or_options.IsPerformanceMeasureOptions()) {
      converted_start = StringOrDouble::FromString("[object Object]");
    } else if (start_or_options.IsDouble()) {
      converted_start = StringOrDouble::FromString(
          String::NumberToStringECMAScript(start_or_options.GetAsDouble()));
    } else if (start_or_options.IsString()) {
      converted_start =
          StringOrDouble::FromString(start_or_options.GetAsString());
    } else {
      DCHECK(start_or_options.IsNull());
      DCHECK(converted_start.IsNull());
    }

    StringOrDouble converted_end;
    if (end.IsString()) {
      converted_end = StringOrDouble::FromString(end.GetAsString());
    } else if (end.IsDouble()) {
      converted_end = StringOrDouble::FromString(
          String::NumberToStringECMAScript(end.GetAsDouble()));
    } else {
      DCHECK(end.IsNull());
      DCHECK(converted_end.IsNull());
    }
    measureInternal(script_state, measure_name, converted_start, converted_end,
                    ScriptValue::CreateNull(script_state), exception_state);
    // Return nullptr to distinguish from L3.
    return nullptr;
  }
}

PerformanceMeasure* Performance::measureInternal(
    ScriptState* script_state,
    const AtomicString& measure_name,
    const StringOrDouble& start,
    const StringOrDouble& end,
    const ScriptValue& detail,
    ExceptionState& exception_state) {
  StringOrDouble original_start = start;
  StringOrDouble original_end = end;

  if (!user_timing_)
    user_timing_ = UserTiming::Create(*this);
  PerformanceMeasure* performance_measure =
      user_timing_->Measure(script_state, measure_name, original_start,
                            original_end, detail, exception_state);
  if (performance_measure)
    NotifyObserversOfEntry(*performance_measure);
  return performance_measure;
}

void Performance::clearMeasures(const AtomicString& measure_name) {
  if (!user_timing_)
    user_timing_ = UserTiming::Create(*this);
  user_timing_->ClearMeasures(measure_name);
}

void Performance::RegisterPerformanceObserver(PerformanceObserver& observer) {
  observer_filter_options_ |= observer.FilterOptions();
  observers_.insert(&observer);
  UpdateLongTaskInstrumentation();
}

void Performance::UnregisterPerformanceObserver(
    PerformanceObserver& old_observer) {
  observers_.erase(&old_observer);
  UpdatePerformanceObserverFilterOptions();
  UpdateLongTaskInstrumentation();
}

void Performance::UpdatePerformanceObserverFilterOptions() {
  observer_filter_options_ = PerformanceEntry::kInvalid;
  for (const auto& observer : observers_) {
    observer_filter_options_ |= observer->FilterOptions();
  }
  UpdateLongTaskInstrumentation();
}

void Performance::NotifyObserversOfEntry(PerformanceEntry& entry) const {
  bool observer_found = false;
  for (auto& observer : observers_) {
    if (observer->FilterOptions() & entry.EntryTypeEnum()) {
      observer->EnqueuePerformanceEntry(entry);
      observer_found = true;
    }
  }
  if (observer_found && entry.EntryTypeEnum() == PerformanceEntry::kPaint)
    UseCounter::Count(GetExecutionContext(), WebFeature::kPaintTimingObserved);
}

void Performance::NotifyObserversOfEntries(PerformanceEntryVector& entries) {
  for (const auto& entry : entries) {
    NotifyObserversOfEntry(*entry.Get());
  }
}

bool Performance::HasObserverFor(
    PerformanceEntry::EntryType filter_type) const {
  return observer_filter_options_ & filter_type;
}

void Performance::ActivateObserver(PerformanceObserver& observer) {
  if (active_observers_.IsEmpty())
    deliver_observations_timer_.StartOneShot(TimeDelta(), FROM_HERE);

  active_observers_.insert(&observer);
}

void Performance::ResumeSuspendedObservers() {
  if (suspended_observers_.IsEmpty())
    return;

  PerformanceObserverVector suspended;
  CopyToVector(suspended_observers_, suspended);
  for (wtf_size_t i = 0; i < suspended.size(); ++i) {
    if (!suspended[i]->ShouldBeSuspended()) {
      suspended_observers_.erase(suspended[i]);
      ActivateObserver(*suspended[i]);
    }
  }
}

void Performance::DeliverObservationsTimerFired(TimerBase*) {
  decltype(active_observers_) observers;
  active_observers_.Swap(observers);
  for (const auto& observer : observers) {
    if (observer->ShouldBeSuspended())
      suspended_observers_.insert(observer);
    else
      observer->Deliver();
  }
}

// static
double Performance::ClampTimeResolution(double time_seconds) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(TimeClamper, clamper, ());
  return clamper.ClampTimeResolution(time_seconds);
}

// static
DOMHighResTimeStamp Performance::MonotonicTimeToDOMHighResTimeStamp(
    TimeTicks time_origin,
    TimeTicks monotonic_time,
    bool allow_negative_value) {
  // Avoid exposing raw platform timestamps.
  if (monotonic_time.is_null() || time_origin.is_null())
    return 0.0;

  double clamped_time_in_seconds =
      ClampTimeResolution(TimeTicksInSeconds(monotonic_time)) -
      ClampTimeResolution(TimeTicksInSeconds(time_origin));
  if (clamped_time_in_seconds < 0 && !allow_negative_value)
    return 0.0;
  return ConvertSecondsToDOMHighResTimeStamp(clamped_time_in_seconds);
}

DOMHighResTimeStamp Performance::MonotonicTimeToDOMHighResTimeStamp(
    TimeTicks monotonic_time) const {
  return MonotonicTimeToDOMHighResTimeStamp(time_origin_, monotonic_time,
                                            false /* allow_negative_value */);
}

DOMHighResTimeStamp Performance::now() const {
  return MonotonicTimeToDOMHighResTimeStamp(CurrentTimeTicks());
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

void Performance::Trace(blink::Visitor* visitor) {
  visitor->Trace(resource_timing_buffer_);
  visitor->Trace(event_timing_buffer_);
  visitor->Trace(navigation_timing_);
  visitor->Trace(user_timing_);
  visitor->Trace(first_paint_timing_);
  visitor->Trace(first_contentful_paint_timing_);
  visitor->Trace(first_input_timing_);
  visitor->Trace(observers_);
  visitor->Trace(active_observers_);
  visitor->Trace(suspended_observers_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
