// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/tracing_controller.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace base {

namespace trace_event {
class TraceConfig;
}  // namespace trace_event

class DictionaryValue;
}  // namespace base

namespace tracing {
class BaseAgent;
}  // namespace tracing

namespace content {

class PerfettoFileTracer;
class TracingDelegate;

class TracingControllerImpl : public TracingController,
                              public mojo::DataPipeDrainer::Client,
                              public tracing::mojom::TracingSessionClient {
 public:
  // Create an endpoint for dumping the trace data to a callback.
  CONTENT_EXPORT static scoped_refptr<TraceDataEndpoint> CreateCallbackEndpoint(
      CompletionCallback callback);

  CONTENT_EXPORT static scoped_refptr<TraceDataEndpoint>
  CreateCompressedStringEndpoint(scoped_refptr<TraceDataEndpoint> endpoint,
                                 bool compress_with_background_priority);

  CONTENT_EXPORT static TracingControllerImpl* GetInstance();

  // Should be called on the UI thread.
  CONTENT_EXPORT TracingControllerImpl();

  // TracingController implementation.
  bool GetCategories(GetCategoriesDoneCallback callback) override;
  bool StartTracing(const base::trace_event::TraceConfig& trace_config,
                    StartTracingDoneCallback callback) override;
  bool StopTracing(const scoped_refptr<TraceDataEndpoint>& endpoint) override;
  bool StopTracing(const scoped_refptr<TraceDataEndpoint>& endpoint,
                   const std::string& agent_label,
                   bool privacy_filtering_enabled = false) override;
  bool GetTraceBufferUsage(GetTraceBufferUsageCallback callback) override;
  bool IsTracing() override;

  // tracing::mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override;
  void OnTracingDisabled() override;

  void OnTracingFailed();

  // For unittests.
  CONTENT_EXPORT void SetTracingDelegateForTesting(
      std::unique_ptr<TracingDelegate> delegate);

  // If command line flags specify startup tracing options, adopts the startup
  // tracing session and relays it to all tracing agents. Note that the local
  // TraceLog has already been enabled at this point by
  // tracing::EnableStartupTracingIfNeeded(), before threads were available.
  // Requires browser threads to have started and a started main message loop.
  void StartStartupTracingIfNeeded();

  // Should be called before browser main loop shutdown. If startup tracing is
  // tracing to a file and is still active, this stops the duration timer if it
  // exists.
  void FinalizeStartupTracingIfNeeded();

  const PerfettoFileTracer* perfetto_file_tracer_for_testing() const {
    return perfetto_file_tracer_.get();
  }

 private:
  friend std::default_delete<TracingControllerImpl>;

  ~TracingControllerImpl() override;
  void AddAgents();
  void ConnectToServiceIfNeeded();
  std::unique_ptr<base::DictionaryValue> GenerateMetadataDict();

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override;
  void OnDataComplete() override;

  void OnReadBuffersComplete();

  void CompleteFlush();

  void InitStartupTracingForDuration();
  void EndStartupTracing();
#if defined(OS_CHROMEOS)
  void OnMachineStatisticsLoaded();
#endif
  base::FilePath GetStartupTraceFileName() const;

  std::unique_ptr<PerfettoFileTracer> perfetto_file_tracer_;
  tracing::mojom::ConsumerHostPtr consumer_host_;
  tracing::mojom::TracingSessionHostPtr tracing_session_host_;
  mojo::Binding<tracing::mojom::TracingSessionClient> binding_{this};
  StartTracingDoneCallback start_tracing_callback_;

  std::vector<std::unique_ptr<tracing::BaseAgent>> agents_;
  std::unique_ptr<TracingDelegate> delegate_;
  std::unique_ptr<base::trace_event::TraceConfig> trace_config_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  scoped_refptr<TraceDataEndpoint> trace_data_endpoint_;
  bool is_data_complete_ = false;
  bool read_buffers_complete_ = false;

  base::FilePath startup_trace_file_;
  // This timer initiates trace file saving.
  base::OneShotTimer startup_trace_timer_;

#if defined(OS_CHROMEOS)
  bool are_statistics_loaded_ = false;
  std::string hardware_class_;
  base::WeakPtrFactory<TracingControllerImpl> weak_ptr_factory_{this};
#endif

  DISALLOW_COPY_AND_ASSIGN(TracingControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
