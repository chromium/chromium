// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_SESSION_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_SESSION_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace perfetto {
namespace protos {
namespace gen {
class TraceStats;
}  // namespace gen
}  // namespace protos
}  // namespace perfetto

namespace tracing {
class TracePacketTokenizer;

// Helpers for deriving information from Perfetto's tracing session statistics.
double COMPONENT_EXPORT(TRACING_CPP)
    GetTraceBufferUsage(const perfetto::protos::gen::TraceStats&);
bool COMPONENT_EXPORT(TRACING_CPP)
    HasLostData(const perfetto::protos::gen::TraceStats&);

void COMPONENT_EXPORT(TRACING_CPP) ReadTraceStats(
    const perfetto::TracingSession::GetTraceStatsCallbackArgs& args,
    base::OnceCallback<void(bool success, float percent_full, bool data_loss)>
        on_stats_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner);

void COMPONENT_EXPORT(TRACING_CPP) ReadTraceAsJson(
    const perfetto::TracingSession::ReadTraceCallbackArgs& args,
    const scoped_refptr<
        base::RefCountedData<std::unique_ptr<TracePacketTokenizer>>>& tokenizer,
    base::OnceCallback<void(std::unique_ptr<std::string>)> on_data_callback,
    base::OnceClosure on_data_complete_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner);

void COMPONENT_EXPORT(TRACING_CPP) ReadTraceAsProtobuf(
    const perfetto::TracingSession::ReadTraceCallbackArgs& args,
    base::OnceCallback<void(std::unique_ptr<std::string>)> on_data_callback,
    base::OnceClosure on_data_complete_callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_SESSION_H_
