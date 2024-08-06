// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"

#include <inttypes.h>

#include <algorithm>

#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void LogRuntimeCallStats(v8::Isolate* isolate) {
  LOG(INFO) << "\n" << RuntimeCallStats::From(isolate)->ToString().Utf8();
}

namespace {
RuntimeCallStats* g_runtime_call_stats_for_testing = nullptr;
}

void RuntimeCallCounter::Dump(TracedValue& value) const {
  value.BeginArray(name_);
  value.PushDouble(count_);
  value.PushDouble(time_.InMicrosecondsF());
  value.EndArray();
}

void RuntimeCallTimer::Start(RuntimeCallCounter* counter,
                             RuntimeCallTimer* parent) {
  DCHECK(!IsRunning());
  counter_ = counter;
  parent_ = parent;
  start_ticks_ = base::TimeTicks(clock_->NowTicks());
  if (parent_)
    parent_->Pause(start_ticks_);
}

RuntimeCallTimer* RuntimeCallTimer::Stop() {
  DCHECK(IsRunning());
  base::TimeTicks now = base::TimeTicks(clock_->NowTicks());
  elapsed_time_ += (now - start_ticks_);
  start_ticks_ = base::TimeTicks();
  counter_->IncrementAndAddTime(elapsed_time_);
  if (parent_)
    parent_->Resume(now);
  return parent_;
}

RuntimeCallStats::RuntimeCallStats(const base::TickClock* clock)
    : clock_(clock) {
  static const char* const names[] = {
#define BINDINGS_COUNTER_NAME(name) "Blink_Bindings_" #name,
      BINDINGS_COUNTERS(BINDINGS_COUNTER_NAME)  //
#undef BINDINGS_COUNTER_NAME
#define GC_COUNTER_NAME(name) "Blink_GC_" #name,
      GC_COUNTERS(GC_COUNTER_NAME)  //
#undef GC_COUNTER_NAME
#define PARSING_COUNTER_NAME(name) "Blink_Parsing_" #name,
      PARSING_COUNTERS(PARSING_COUNTER_NAME)  //
#undef PARSING_COUNTER_NAME
#define STYLE_COUNTER_NAME(name) "Blink_Style_" #name,
      STYLE_COUNTERS(STYLE_COUNTER_NAME)  //
#undef STYLE_COUNTER_NAME
#define LAYOUT_COUNTER_NAME(name) "Blink_Layout_" #name,
      LAYOUT_COUNTERS(LAYOUT_COUNTER_NAME)  //
#undef STYLE_COUNTER_NAME
#define COUNTER_NAME(name) "Blink_" #name,
      CALLBACK_COUNTERS(COUNTER_NAME)  //
      EXTRA_COUNTERS(COUNTER_NAME)
#undef COUNTER_NAME
  };

  for (int i = 0; i < number_of_counters_; i++) {
    counters_[i] = RuntimeCallCounter(names[i]);
  }
}

// static
RuntimeCallStats* RuntimeCallStats::From(v8::Isolate* isolate) {
  if (g_runtime_call_stats_for_testing)
    return g_runtime_call_stats_for_testing;
  return V8PerIsolateData::From(isolate)->GetRuntimeCallStats();
}

void RuntimeCallStats::Reset() {
  for (int i = 0; i < number_of_counters_; i++) {
    counters_[i].Reset();
  }

#if BUILDFLAG(RCS_COUNT_EVERYTHING)
  for (const auto& counter : counter_map_.Values()) {
    counter->Reset();
  }
#endif
}

void RuntimeCallStats::Dump(TracedValue& value) const {
  for (int i = 0; i < number_of_counters_; i++) {
    if (counters_[i].GetCount() > 0)
      counters_[i].Dump(value);
  }

#if BUILDFLAG(RCS_COUNT_EVERYTHING)
  for (const auto& counter : counter_map_.Values()) {
    if (counter->GetCount() > 0)
      counter->Dump(value);
  }
#endif
}

namespace {
const char row_format[] = "%-55s  %8" PRIu64 "  %9.3f\n";
}

String RuntimeCallStats::ToString() const {
  StringBuilder builder;
  builder.Append("Runtime Call Stats for Blink \n");
  builder.Append(
      "Name                                                    Count     Time "
      "(ms)\n\n");
  for (int i = 0; i < number_of_counters_; i++) {
    const RuntimeCallCounter* counter = &counters_[i];
    builder.AppendFormat(row_format, counter->GetName(), counter->GetCount(),
                         counter->GetTime().InMillisecondsF());
  }

#if BUILDFLAG(RCS_COUNT_EVERYTHING)
  AddCounterMapStatsToBuilder(builder);
#endif

  return builder.ToString();
}

// static
void RuntimeCallStats::SetRuntimeCallStatsForTesting() {
  DEFINE_STATIC_LOCAL(RuntimeCallStats, s_rcs_for_testing,
                      (base::DefaultTickClock::GetInstance()));
  g_runtime_call_stats_for_testing =
      static_cast<RuntimeCallStats*>(&s_rcs_for_testing);
}

// static
void RuntimeCallStats::ClearRuntimeCallStatsForTesting() {
  g_runtime_call_stats_for_testing = nullptr;
}

// This function exists to remove runtime_enabled_features.h dependnency from
// runtime_call_stats.h.
bool RuntimeCallStats::IsEnabled() {
  return RuntimeEnabledFeatures::BlinkRuntimeCallStatsEnabled();
}

#if BUILDFLAG(RCS_COUNT_EVERYTHING)
RuntimeCallCounter* RuntimeCallStats::GetCounter(const char* name) {
  CounterMap::iterator it = counter_map_.find(name);
  if (it != counter_map_.end())
    return it->value.get();
  return counter_map_.insert(name, std::make_unique<RuntimeCallCounter>(name))
      .stored_value->value.get();
}

Vector<RuntimeCallCounter*> RuntimeCallStats::CounterMapToSortedArray() const {
  Vector<RuntimeCallCounter*> counters;
  for (const auto& counter : counter_map_.Values()) {
    counters.push_back(counter.get());
  }
  auto comparator = [](RuntimeCallCounter* a, RuntimeCallCounter* b) {
    return a->GetCount() == b->GetCount()
               ? strcmp(a->GetName(), b->GetName()) < 0
               : a->GetCount() < b->GetCount();
  };
  std::sort(counters.begin(), counters.end(), comparator);
  return counters;
}

void RuntimeCallStats::AddCounterMapStatsToBuilder(
    StringBuilder& builder) const {
  builder.AppendFormat("\nNumber of counters in map: %u\n\n",
                       counter_map_.size());
  for (RuntimeCallCounter* counter : CounterMapToSortedArray()) {
    builder.AppendFormat(row_format, counter->GetName(), counter->GetCount(),
                         counter->GetTime().InMillisecondsF());
  }
}
#endif

constexpr const char* RuntimeCallStatsScopedTracer::s_category_group_ =
    TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats");
constexpr const char* RuntimeCallStatsScopedTracer::s_name_ =
    "BlinkRuntimeCallStats";

void RuntimeCallStatsScopedTracer::AddBeginTraceEventIfEnabled(
    v8::Isolate* isolate) {
  bool category_group_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(s_category_group_,
                                     &category_group_enabled);
  if (!category_group_enabled) [[likely]] {
    return;
  }

  RuntimeCallStats* stats = RuntimeCallStats::From(isolate);
  if (stats->InUse())
    return;
  stats_ = stats;
  stats_->Reset();
  stats_->SetInUse(true);
  TRACE_EVENT_BEGIN0(s_category_group_, s_name_);
}

void RuntimeCallStatsScopedTracer::AddEndTraceEvent() {
  auto value = std::make_unique<TracedValue>();
  stats_->Dump(*value);
  stats_->SetInUse(false);
  TRACE_EVENT_END1(s_category_group_, s_name_, "runtime-call-stats",
                   std::move(value));
}

}  // namespace blink
