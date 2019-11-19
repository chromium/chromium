// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_THREAD_LOCAL_EVENT_SINK_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_THREAD_LOCAL_EVENT_SINK_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

namespace perfetto {
class StartupTraceWriter;
}

namespace tracing {

// Base class for a per-thread TraceEvent sink, which writes TraceEvents into
// TracePacket protos in the SMB.
class COMPONENT_EXPORT(TRACING_CPP) ThreadLocalEventSink {
 public:
  ThreadLocalEventSink(
      std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
      uint32_t session_id,
      bool disable_interning);

  virtual ~ThreadLocalEventSink();

  // TODO(oysteine): Adding trace events to Perfetto will stall in some
  // situations, specifically when we overflow the buffer and need to make a
  // sync call to flush it, and we're running on the same thread as the service.
  // The short-term fix (while we're behind a flag) is to run the service on its
  // own thread, the longer term fix is most likely to not go via Mojo in that
  // specific case.
  virtual void AddTraceEvent(base::trace_event::TraceEvent* trace_event,
                             base::trace_event::TraceEventHandle* handle) = 0;

  virtual void UpdateDuration(
      base::trace_event::TraceEventHandle handle,
      const base::TimeTicks& now,
      const base::ThreadTicks& thread_now,
      base::trace_event::ThreadInstructionCount thread_instruction_now) = 0;

  virtual void Flush() = 0;

  uint32_t session_id() const { return session_id_; }

 protected:
  std::unique_ptr<perfetto::StartupTraceWriter> trace_writer_;
  uint32_t session_id_;
  bool disable_interning_;
  uint32_t sink_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadLocalEventSink);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_THREAD_LOCAL_EVENT_SINK_H_
