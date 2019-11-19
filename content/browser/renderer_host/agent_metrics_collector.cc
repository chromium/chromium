// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/agent_metrics_collector.h"

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

const char kAgentsByTimeHistogram[] = "PerformanceManager.AgentsByTime";
const char kAgentsUniqueByTimeHistogram[] =
    "PerformanceManager.AgentsUniqueByTime";

constexpr base::TimeDelta kReportingInterval = base::TimeDelta::FromMinutes(5);

// This object is allocated statically and serves to combine and store all the
// per-process counts.
class Aggregator {
 public:
  Aggregator() { time_last_reported_ = base::TimeTicks::Now(); }

  void ComputeAndReport() {
    int total = 0;
    int unique = 0;
    std::set<std::string> sites;
    for (auto const& pair : process_to_agent_sites_) {
      auto& sites_vector = pair.second;
      total += sites_vector.size();
      for (auto& agent : sites_vector) {
        if (agent.empty()) {
          // An empty string indicates non-tuple origins; don't consolidate
          // those.
          ++unique;
          continue;
        }
        sites.insert(agent);
      }
    }

    unique += sites.size();

    // This computation and reporting is based on the one in:
    // chrome/browser/performance_manager/observers/isolation_context_metrics.cc
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta elapsed = now - time_last_reported_;

    // Account for edge cases like hibernate/sleep. See
    // GetSecondsSinceLastReportAndUpdate in isolation_context_metrics.cc. This
    // might lose some data just before a sleep but that's probably ok.
    if (elapsed >= 2 * kReportingInterval) {
      time_last_reported_ = now;
      return;
    }

    int seconds = static_cast<int>(std::round(elapsed.InSecondsF()));
    if (seconds <= 0)
      return;

    // Note: renderers report their metrics by pushing sending them over every 5
    // minutes so we don't need to setup our own timer here, we can just rely on
    // those updates reaching here.

    STATIC_HISTOGRAM_POINTER_BLOCK(
        kAgentsByTimeHistogram, AddCount(total, seconds),
        base::Histogram::FactoryGet(
            kAgentsByTimeHistogram, 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));

    STATIC_HISTOGRAM_POINTER_BLOCK(
        kAgentsUniqueByTimeHistogram, AddCount(unique, seconds),
        base::Histogram::FactoryGet(
            kAgentsUniqueByTimeHistogram, 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));

    time_last_reported_ = now;
  }

  void AddPerRendererData(int process_id,
                          blink::mojom::AgentMetricsDataPtr data) {
    ComputeAndReport();
    process_to_agent_sites_[process_id] = std::move(data->agents);
  }

  void RemovePerRendererData(int process_id) {
    auto itr = process_to_agent_sites_.find(process_id);
    if (itr == process_to_agent_sites_.end())
      return;

    ComputeAndReport();
    process_to_agent_sites_.erase(itr);
  }

 private:
  // A map from each renderer process' id to a collection of agent strings
  // hosted in that renderer. The string will be the security origin's
  // |protocol| + "://" + |registrable domain|. If the agent was opaque or
  // otherwise not associated with a domain (e.g. file://) then it will be
  // added as an empty string. These agents should not be coalesced.
  using ProcessToAgentsMapType = std::map<int, std::vector<std::string>>;
  ProcessToAgentsMapType process_to_agent_sites_;

  base::TimeTicks time_last_reported_;
};

Aggregator& GetAggregator() {
  static base::NoDestructor<Aggregator> a;
  return *a;
}

}  // namespace

AgentMetricsCollectorHost::AgentMetricsCollectorHost(
    int id,
    mojo::PendingReceiver<blink::mojom::AgentMetricsCollectorHost> receiver)
    : process_id_(id), receiver_(this, std::move(receiver)) {}

AgentMetricsCollectorHost::~AgentMetricsCollectorHost() {}

void AgentMetricsCollectorHost::ReportRendererMetrics(
    blink::mojom::AgentMetricsDataPtr data) {
  GetAggregator().AddPerRendererData(process_id_, std::move(data));
}

void AgentMetricsCollectorHost::RemoveRendererData() {
  GetAggregator().RemovePerRendererData(process_id_);
}

void AgentMetricsCollectorHost::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  RemoveRendererData();
  host->RemoveObserver(this);
}

void AgentMetricsCollectorHost::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  RemoveRendererData();
}

}  // namespace content
