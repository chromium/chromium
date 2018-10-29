// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/tracing_controller.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace base {

namespace trace_event {
class TraceConfig;
}  // namespace trace_event

class DictionaryValue;
class RefCountedString;
}  // namespace base

namespace tracing {
class TraceEventAgent;
}  // namespace tracing

namespace content {

class TracingDelegate;
class TracingUI;

class TracingControllerImpl : public TracingController,
                              public mojo::DataPipeDrainer::Client {
 public:
  // Create an endpoint for dumping the trace data to a callback.
  CONTENT_EXPORT static scoped_refptr<TraceDataEndpoint> CreateCallbackEndpoint(
      const base::Callback<void(std::unique_ptr<const base::DictionaryValue>,
                                base::RefCountedString*)>& callback);

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
                   const std::string& agent_label) override;
  bool GetTraceBufferUsage(GetTraceBufferUsageCallback callback) override;
  bool IsTracing() const override;

  void RegisterTracingUI(TracingUI* tracing_ui);
  void UnregisterTracingUI(TracingUI* tracing_ui);

  CONTENT_EXPORT tracing::TraceEventAgent* GetTraceEventAgent() const;

 private:
  friend std::default_delete<TracingControllerImpl>;

  ~TracingControllerImpl() override;
  void AddAgents();
  std::unique_ptr<base::DictionaryValue> GenerateMetadataDict() const;

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override;
  void OnDataComplete() override;

  void OnMetadataAvailable(base::Value metadata);

  void CompleteFlush();

  tracing::mojom::AgentRegistryPtr agent_registry_;
  tracing::mojom::CoordinatorPtr coordinator_;
  std::vector<std::unique_ptr<tracing::mojom::Agent>> agents_;
  std::unique_ptr<tracing::TraceEventAgent> trace_event_agent_;
  std::unique_ptr<TracingDelegate> delegate_;
  std::unique_ptr<base::trace_event::TraceConfig> trace_config_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  scoped_refptr<TraceDataEndpoint> trace_data_endpoint_;
  std::unique_ptr<base::DictionaryValue> filtered_metadata_;
  std::set<TracingUI*> tracing_uis_;
  bool is_data_complete_ = false;
  bool is_metadata_available_ = false;

  DISALLOW_COPY_AND_ASSIGN(TracingControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
