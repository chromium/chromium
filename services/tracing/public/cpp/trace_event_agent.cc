// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_event_agent.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace tracing {

// static
TraceEventAgent* TraceEventAgent::GetInstance() {
  static base::NoDestructor<TraceEventAgent> instance;
  return instance.get();
}

TraceEventAgent::TraceEventAgent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // These filters are used by TraceLog in the legacy tracing system and JSON
  // exporter (only in tracing service) in perfetto bcakend.
  if (base::trace_event::TraceLog::GetInstance()
          ->GetArgumentFilterPredicate()
          .is_null()) {
    base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
        base::BindRepeating(&IsTraceEventArgsWhitelisted));
    base::trace_event::TraceLog::GetInstance()->SetMetadataFilterPredicate(
        base::BindRepeating(&IsMetadataWhitelisted));
  }

  PerfettoTracedProcess::Get()->AddDataSource(
      TraceEventDataSource::GetInstance());
  TracingSamplerProfiler::RegisterDataSource();
}

TraceEventAgent::~TraceEventAgent() = default;

void TraceEventAgent::GetCategories(std::set<std::string>* category_set) {
  for (size_t i = base::trace_event::BuiltinCategories::kVisibleCategoryStart;
       i < base::trace_event::BuiltinCategories::Size(); ++i) {
    category_set->insert(base::trace_event::BuiltinCategories::At(i));
  }
}

void TraceEventAgent::AddMetadataGeneratorFunction(
    MetadataGeneratorFunction generator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metadata_generator_functions_.push_back(generator);

  TraceEventMetadataSource::GetInstance()->AddGeneratorFunction(generator);
}

}  // namespace tracing
