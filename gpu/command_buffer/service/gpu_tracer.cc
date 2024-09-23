// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gpu_tracer.h"

#include <stddef.h>
#include <stdint.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/gpu_timing.h"

namespace gpu {
namespace gles2 {

constexpr const char* kGpuTraceSourceNames[] = {
    "TraceCHROMIUM",  // kTraceCHROMIUM,
    "TraceCmd",       // kTraceDecoder,
    "Disjoint",       // kTraceDisjoint, // Used internally.
};
static_assert(NUM_TRACER_SOURCES == std::size(kGpuTraceSourceNames),
              "Trace source names must match enumeration.");

TraceMarker::TraceMarker(const std::string& category, const std::string& name)
    : category_(category), name_(name), trace_(nullptr) {}

TraceMarker::TraceMarker(const TraceMarker& other) = default;

TraceMarker::~TraceMarker() = default;

TraceOutputter::TraceOutputter() : named_thread_("Dummy Trace") {}

TraceOutputter::TraceOutputter(const std::string& name) : named_thread_(name) {
}

TraceOutputter::~TraceOutputter() = default;

void TraceOutputter::TraceDevice(GpuTracerSource source,
                                 const std::string& category,
                                 const std::string& name,
                                 int64_t start_time,
                                 int64_t end_time) {
  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);
  DCHECK(end_time >= start_time) << end_time << " >= " << start_time;

  if (named_thread_id_ == base::kInvalidThreadId) {
    named_thread_.Start();
    named_thread_id_ = named_thread_.GetThreadId();
    named_thread_.Stop();
  }

  TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP2(
      TRACE_DISABLED_BY_DEFAULT("gpu.device"), name.c_str(),
      local_trace_device_id_, named_thread_id_,
      base::TimeTicks::FromInternalValue(start_time), "gl_category",
      category.c_str(), "channel", kGpuTraceSourceNames[source]);

  // Time stamps are inclusive, since the traces are durations we subtract
  // 1 microsecond from the end time to make the trace markers show up cleaner.
  if (end_time > start_time)
    end_time -= 1;
  TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP2(
      TRACE_DISABLED_BY_DEFAULT("gpu.device"), name.c_str(),
      local_trace_device_id_, named_thread_id_,
      base::TimeTicks::FromInternalValue(end_time), "gl_category",
      category.c_str(), "channel", kGpuTraceSourceNames[source]);
  ++local_trace_device_id_;
}

void TraceOutputter::TraceServiceBegin(GpuTracerSource source,
                                       const std::string& category,
                                       const std::string& name) {
  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);
  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TTS2(
      TRACE_DISABLED_BY_DEFAULT("gpu.service"),
      name.c_str(), local_trace_service_id_,
      "gl_category", category.c_str(),
      "channel", kGpuTraceSourceNames[source]);

  trace_service_id_stack_[source].push(local_trace_service_id_);
  ++local_trace_service_id_;
}

void TraceOutputter::TraceServiceEnd(GpuTracerSource source,
                                     const std::string& category,
                                     const std::string& name) {
  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);
  DCHECK(!trace_service_id_stack_[source].empty());
  const uint64_t local_trace_id = trace_service_id_stack_[source].top();
  trace_service_id_stack_[source].pop();

  TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TTS2(
      TRACE_DISABLED_BY_DEFAULT("gpu.service"),
      name.c_str(), local_trace_id,
      "gl_category", category.c_str(),
      "channel", kGpuTraceSourceNames[source]);
}

GPUTrace::GPUTrace(Outputter* outputter,
                   gl::GPUTimingClient* gpu_timing_client,
                   const GpuTracerSource source,
                   const std::string& category,
                   const std::string& name,
                   const bool tracing_service,
                   const bool tracing_device)
    : source_(source),
      category_(category),
      name_(name),
      outputter_(outputter),
      service_enabled_(tracing_service),
      device_enabled_(tracing_device) {
  if (tracing_device && gpu_timing_client->IsAvailable())
    gpu_timer_ = gpu_timing_client->CreateGPUTimer(false);
}

GPUTrace::~GPUTrace() = default;

void GPUTrace::Destroy(bool have_context) {
  if (gpu_timer_.get()) {
    gpu_timer_->Destroy(have_context);
  }
}

void GPUTrace::Start() {
  if (service_enabled_) {
    outputter_->TraceServiceBegin(source_, category_, name_);
  }
  if (gpu_timer_.get()) {
    gpu_timer_->Start();
  }
}

void GPUTrace::End() {
  if (gpu_timer_.get()) {
    gpu_timer_->End();
  }
  if (service_enabled_) {
    outputter_->TraceServiceEnd(source_, category_, name_);
  }
}

bool GPUTrace::IsAvailable() {
  return !gpu_timer_.get() || gpu_timer_->IsAvailable();
}

void GPUTrace::Process() {
  if (gpu_timer_.get() && device_enabled_) {
    DCHECK(IsAvailable());

    int64_t start = 0;
    int64_t end = 0;
    gpu_timer_->GetStartEndTimestamps(&start, &end);
    outputter_->TraceDevice(source_, category_, name_, start, end);
  }
}

GPUTracer::GPUTracer(DecoderContext* decoder, bool context_is_gl)
    : gpu_trace_srv_category_(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.service"))),
      gpu_trace_dev_category_(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.device"))),
      decoder_(decoder) {
  DCHECK(decoder_);
  gl::GLContext* gl_context = decoder_->GetGLContext();
  if (context_is_gl && gl_context) {
    can_trace_dev_ = true;
    gpu_timing_client_ = gl_context->CreateGPUTimingClient();
    disjoint_time_ = gpu_timing_client_->GetCurrentCPUTime();
  } else {
    can_trace_dev_ = false;
    // TODO(crbug.com/40655549): GPUTiming should support backends other than
    // GL.
    gpu_timing_client_ = nullptr;
  }
  outputter_ = decoder_->outputter();
}

GPUTracer::~GPUTracer() = default;

void GPUTracer::Destroy(bool have_context) {
  ClearOngoingTraces(have_context);
}

bool GPUTracer::BeginDecoding() {
  if (gpu_executing_)
    return false;

  gpu_executing_ = true;
  if (IsTracing()) {
    CheckDisjointStatus();
    // Begin a Trace for all active markers
    for (int n = 0; n < NUM_TRACER_SOURCES; n++) {
      for (size_t i = 0; i < markers_[n].size(); i++) {
        began_device_traces_ |= is_gpu_device_tracing_enabled();
        TraceMarker& trace_marker = markers_[n][i];
        trace_marker.trace_ = new GPUTrace(
            outputter_, gpu_timing_client_.get(),
            static_cast<GpuTracerSource>(n), trace_marker.category_,
            trace_marker.name_, is_gpu_service_tracing_enabled(),
            is_gpu_device_tracing_enabled());
        trace_marker.trace_->Start();
      }
    }
  }
  return true;
}

bool GPUTracer::EndDecoding() {
  if (!gpu_executing_)
    return false;

  // End Trace for all active markers
  if (IsTracing()) {
    for (int n = 0; n < NUM_TRACER_SOURCES; n++) {
      if (!markers_[n].empty()) {
        for (int i = static_cast<int>(markers_[n].size()) - 1; i >= 0; --i) {
          TraceMarker& marker = markers_[n][i];
          if (marker.trace_.get()) {
            marker.trace_->End();

            finished_traces_.push_back(marker.trace_);
            marker.trace_.reset();
          }
        }
      }
    }
  }

  gpu_executing_ = false;
  return true;
}

bool GPUTracer::Begin(const std::string& category, const std::string& name,
                      GpuTracerSource source) {
  if (!gpu_executing_)
    return false;

  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);

  // Push new marker from given 'source'
  markers_[source].push_back(TraceMarker(category, name));

  // Create trace
  if (IsTracing()) {
    began_device_traces_ |= is_gpu_device_tracing_enabled();
    scoped_refptr<GPUTrace> trace = new GPUTrace(
        outputter_, gpu_timing_client_.get(), source, category, name,
        is_gpu_service_tracing_enabled(), is_gpu_device_tracing_enabled());
    trace->Start();
    markers_[source].back().trace_ = trace;
  }

  return true;
}

bool GPUTracer::End(GpuTracerSource source) {
  if (!gpu_executing_)
    return false;

  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);

  // Pop last marker with matching 'source'
  if (!markers_[source].empty()) {
    scoped_refptr<GPUTrace> trace = markers_[source].back().trace_;
    if (trace.get()) {
      if (IsTracing()) {
        trace->End();
      }

      finished_traces_.push_back(trace);
    }

    markers_[source].pop_back();
    return true;
  }
  return false;
}

bool GPUTracer::HasTracesToProcess() {
  return !finished_traces_.empty();
}

void GPUTracer::ProcessTraces() {
  if (gpu_timing_client_ && !gpu_timing_client_->IsAvailable()) {
    while (!finished_traces_.empty()) {
      finished_traces_.front()->Destroy(false);
      finished_traces_.pop_front();
    }
    return;
  }

  TRACE_EVENT0("gpu", "GPUTracer::ProcessTraces");

  // Make owning decoder's GL context current
  if (!decoder_->MakeCurrent()) {
    // Skip subsequent GL calls if MakeCurrent fails
    ClearOngoingTraces(false);
    return;
  }

  // Check available traces.
  int available_traces = 0;
  for (scoped_refptr<GPUTrace>& trace : finished_traces_) {
    if (trace->IsDeviceTraceEnabled() && !trace->IsAvailable()) {
      break;
    }
    available_traces++;
  }

  // Clear pending traces if there were are any errors including disjoint.
  if (CheckDisjointStatus()) {
    ClearOngoingTraces(true);
  } else {
    for (int i = 0; i < available_traces; ++i) {
      scoped_refptr<GPUTrace>& trace = finished_traces_.front();
      trace->Process();
      trace->Destroy(true);
      finished_traces_.pop_front();
    }
  }

  // When `can_trace_dev_` is false, there might be no current context and
  // calling `glGetError()` might lead to a crash. To avoid that, skip calling
  // the function in that case.
  DCHECK(!can_trace_dev_ || GL_NO_ERROR == glGetError());
}

bool GPUTracer::IsTracing() {
  return is_gpu_service_tracing_enabled() || is_gpu_device_tracing_enabled();
}

const std::string& GPUTracer::CurrentCategory(GpuTracerSource source) const {
  if (source >= 0 &&
      source < NUM_TRACER_SOURCES &&
      !markers_[source].empty()) {
    return markers_[source].back().category_;
  }
  return base::EmptyString();
}

const std::string& GPUTracer::CurrentName(GpuTracerSource source) const {
  if (source >= 0 &&
      source < NUM_TRACER_SOURCES &&
      !markers_[source].empty()) {
    return markers_[source].back().name_;
  }
  return base::EmptyString();
}

bool GPUTracer::CheckDisjointStatus() {
  if (!is_gpu_device_tracing_enabled())
    return false;

  const int64_t current_time = gpu_timing_client_->GetCurrentCPUTime();
  bool status = gpu_timing_client_->CheckAndResetTimerErrors();
  if (status && began_device_traces_) {
    // Log disjoint event if we have active traces.
    const std::string unique_disjoint_name =
        base::StringPrintf("DisjointEvent-%p", this);
    outputter_->TraceDevice(kTraceDisjoint,
                            "DisjointEvent",
                            unique_disjoint_name,
                            disjoint_time_,
                            current_time);
  }
  disjoint_time_ = current_time;
  return status;
}

void GPUTracer::ClearOngoingTraces(bool have_context) {
  for (int n = 0; n < NUM_TRACER_SOURCES; n++) {
    for (size_t i = 0; i < markers_[n].size(); i++) {
      TraceMarker& marker = markers_[n][i];
      if (marker.trace_.get()) {
        marker.trace_->Destroy(have_context);
        marker.trace_.reset();
      }
    }
  }

  while (!finished_traces_.empty()) {
    finished_traces_.front()->Destroy(have_context);
    finished_traces_.pop_front();
  }
}

}  // namespace gles2
}  // namespace gpu
