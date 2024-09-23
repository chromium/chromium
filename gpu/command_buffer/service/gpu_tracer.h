// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GPUTrace class.
#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_TRACER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_TRACER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/stack.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class GPUTimingClient;
class GPUTimer;
}  // namespace gl

namespace gpu {

class DecoderContext;

namespace gles2 {

class Outputter;
class GPUTrace;

// Id used to keep trace namespaces separate
enum GpuTracerSource {
  kTraceGroupInvalid = -1,

  kTraceCHROMIUM,
  kTraceDecoder,
  kTraceDisjoint,  // Used internally.

  NUM_TRACER_SOURCES
};

// Marker structure for a Trace.
struct TraceMarker {
  TraceMarker(const std::string& category, const std::string& name);
  TraceMarker(const TraceMarker& other);
  ~TraceMarker();

  std::string category_;
  std::string name_;
  scoped_refptr<GPUTrace> trace_;
};

// Traces GPU Commands.
class GPU_GLES2_EXPORT GPUTracer {
 public:
  explicit GPUTracer(DecoderContext* decoder, bool context_is_gl = true);

  GPUTracer(const GPUTracer&) = delete;
  GPUTracer& operator=(const GPUTracer&) = delete;

  virtual ~GPUTracer();

  void Destroy(bool have_context);

  // Scheduled processing in decoder begins.
  bool BeginDecoding();

  // Scheduled processing in decoder ends.
  bool EndDecoding();

  // Begin a trace marker.
  bool Begin(const std::string& category, const std::string& name,
             GpuTracerSource source);

  // End the last started trace marker.
  bool End(GpuTracerSource source);

  bool HasTracesToProcess();
  void ProcessTraces();

  virtual bool IsTracing();

  // Retrieve the name of the current open trace.
  // Returns empty string if no current open trace.
  const std::string& CurrentCategory(GpuTracerSource source) const;
  const std::string& CurrentName(GpuTracerSource source) const;

 protected:
  bool is_gpu_service_tracing_enabled() {
    return *gpu_trace_srv_category_ != 0;
  }
  bool is_gpu_device_tracing_enabled() {
    return *gpu_trace_dev_category_ != 0 && can_trace_dev_;
  }

  scoped_refptr<gl::GPUTimingClient> gpu_timing_client_;
  raw_ptr<const unsigned char> gpu_trace_srv_category_;
  raw_ptr<const unsigned char> gpu_trace_dev_category_;
  // Disable gpu.device tracing if context is corrupted or not GL.
  bool can_trace_dev_;

 private:
  bool CheckDisjointStatus();
  void ClearOngoingTraces(bool have_context);

  raw_ptr<Outputter> outputter_ = nullptr;
  std::vector<TraceMarker> markers_[NUM_TRACER_SOURCES];
  base::circular_deque<scoped_refptr<GPUTrace>> finished_traces_;
  raw_ptr<DecoderContext> decoder_;
  int64_t disjoint_time_ = 0;
  bool gpu_executing_ = false;
  bool began_device_traces_ = false;
};

class GPU_GLES2_EXPORT Outputter {
 public:
  virtual ~Outputter() = default;

  virtual void TraceDevice(GpuTracerSource source,
                           const std::string& category,
                           const std::string& name,
                           int64_t start_time,
                           int64_t end_time) = 0;

  virtual void TraceServiceBegin(GpuTracerSource source,
                                 const std::string& category,
                                 const std::string& name) = 0;

  virtual void TraceServiceEnd(GpuTracerSource source,
                               const std::string& category,
                               const std::string& name) = 0;
};

class GPU_GLES2_EXPORT TraceOutputter : public Outputter {
 public:
  TraceOutputter();
  explicit TraceOutputter(const std::string& name);

  TraceOutputter(const TraceOutputter&) = delete;
  TraceOutputter& operator=(const TraceOutputter&) = delete;

  ~TraceOutputter() override;

  void TraceDevice(GpuTracerSource source,
                   const std::string& category,
                   const std::string& name,
                   int64_t start_time,
                   int64_t end_time) override;

  void TraceServiceBegin(GpuTracerSource source,
                         const std::string& category,
                         const std::string& name) override;

  void TraceServiceEnd(GpuTracerSource source,
                       const std::string& category,
                       const std::string& name) override;

 private:
  base::Thread named_thread_;
  base::PlatformThreadId named_thread_id_ = base::kInvalidThreadId;
  uint64_t local_trace_device_id_ = 0;
  uint64_t local_trace_service_id_ = 0;

  base::stack<uint64_t> trace_service_id_stack_[NUM_TRACER_SOURCES];
};

class GPU_GLES2_EXPORT GPUTrace : public base::RefCounted<GPUTrace> {
 public:
  GPUTrace(Outputter* outputter,
           gl::GPUTimingClient* gpu_timing_client,
           const GpuTracerSource source,
           const std::string& category,
           const std::string& name,
           const bool tracing_service,
           const bool tracing_device);

  GPUTrace(const GPUTrace&) = delete;
  GPUTrace& operator=(const GPUTrace&) = delete;

  void Destroy(bool have_context);

  void Start();
  void End();
  bool IsAvailable();
  bool IsServiceTraceEnabled() const { return service_enabled_; }
  bool IsDeviceTraceEnabled() const { return device_enabled_; }
  void Process();

 private:
  ~GPUTrace();

  void Output();

  friend class base::RefCounted<GPUTrace>;

  const GpuTracerSource source_ = kTraceGroupInvalid;
  const std::string category_;
  const std::string name_;
  raw_ptr<Outputter> outputter_ = nullptr;
  std::unique_ptr<gl::GPUTimer> gpu_timer_;
  const bool service_enabled_ = false;
  const bool device_enabled_ = false;
};

class ScopedGPUTrace {
 public:
  ScopedGPUTrace(GPUTracer* gpu_tracer,
                 GpuTracerSource source,
                 const std::string& category,
                 const std::string& name)
      : gpu_tracer_(gpu_tracer), source_(source) {
    gpu_tracer_->Begin(category, name, source_);
  }

  ~ScopedGPUTrace() { gpu_tracer_->End(source_); }

 private:
  raw_ptr<GPUTracer> gpu_tracer_;
  GpuTracerSource source_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_TRACER_H_
