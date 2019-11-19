// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_AGENT_METRICS_COLLECTOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_AGENT_METRICS_COLLECTOR_H_

#include "content/common/renderer_host.mojom.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/agents/agent_metrics.mojom.h"

namespace content {

// Collects and reports metrics about the number of agents being hosted in the
// various renderer processes. This is the browser-side of the Blink-based
// execution_context/agent_metrics_collector.
class CONTENT_EXPORT AgentMetricsCollectorHost
    : public blink::mojom::AgentMetricsCollectorHost,
      public RenderProcessHostObserver {
 public:
  AgentMetricsCollectorHost(
      int id,
      mojo::PendingReceiver<blink::mojom::AgentMetricsCollectorHost> receiver);
  ~AgentMetricsCollectorHost() override;

  void ReportRendererMetrics(blink::mojom::AgentMetricsDataPtr data) override;
  void RemoveRendererData();

  // RenderProcessHostObserver implementation
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

 private:
  int process_id_;

  mojo::Receiver<blink::mojom::AgentMetricsCollectorHost> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_AGENT_METRICS_COLLECTOR_H_
