// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "content/public/browser/tracing_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"

namespace perfetto::protos::pbzero {
class TracePacket;
}  // namespace perfetto::protos::pbzero

namespace base::trace_event {
class TraceConfig;
}  // namespace base::trace_event

namespace tracing {
class BaseAgent;
}  // namespace tracing

namespace content {

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

  TracingControllerImpl(const TracingControllerImpl&) = delete;
  TracingControllerImpl& operator=(const TracingControllerImpl&) = delete;

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
  void OnTracingDisabled(bool tracing_succeeded) override;

  void OnTracingFailed();

 private:
  friend std::default_delete<TracingControllerImpl>;

  ~TracingControllerImpl() override;
  void AddAgents();
  void ConnectToServiceIfNeeded();
  std::optional<base::Value::Dict> GenerateMetadataDict();
  void GenerateMetadataPacket(perfetto::protos::pbzero::TracePacket* packet,
                              bool privacy_filtering_enabled);
  void GenerateMetadataPacketFieldTrials(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata_proto,
      bool privacy_filtering_enabled);

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  void OnReadBuffersComplete();

  void CompleteFlush();

  void InitStartupTracingForDuration();
  void EndStartupTracing();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnMachineStatisticsLoaded();
#endif

  mojo::Remote<tracing::mojom::ConsumerHost> consumer_host_;
  mojo::Remote<tracing::mojom::TracingSessionHost> tracing_session_host_;
  mojo::Receiver<tracing::mojom::TracingSessionClient> receiver_{this};
  StartTracingDoneCallback start_tracing_callback_;

  std::vector<std::unique_ptr<tracing::BaseAgent>> agents_;
  std::unique_ptr<base::trace_event::TraceConfig> trace_config_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  scoped_refptr<TraceDataEndpoint> trace_data_endpoint_;
  bool is_data_complete_ = false;
  bool read_buffers_complete_ = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool are_statistics_loaded_ = false;
  std::string hardware_class_;
  base::WeakPtrFactory<TracingControllerImpl> weak_ptr_factory_{this};
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
