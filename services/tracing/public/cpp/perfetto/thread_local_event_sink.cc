// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/thread_local_event_sink.h"

#include <atomic>
#include <utility>

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/startup_trace_writer.h"

namespace tracing {

ThreadLocalEventSink::ThreadLocalEventSink(
    std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
    uint32_t session_id,
    bool disable_interning)
    : trace_writer_(std::move(trace_writer)),
      session_id_(session_id),
      disable_interning_(disable_interning) {
  static std::atomic<uint32_t> g_sink_id_counter{0};
  sink_id_ = ++g_sink_id_counter;
}

ThreadLocalEventSink::~ThreadLocalEventSink() {
  // Subclass has already destroyed its message handles at this point.
  TraceEventDataSource::GetInstance()->ReturnTraceWriter(
      std::move(trace_writer_));
}

}  // namespace tracing
