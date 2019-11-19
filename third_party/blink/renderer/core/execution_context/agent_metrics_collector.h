// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_METRICS_COLLECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_METRICS_COLLECTOR_H_

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/agents/agent_metrics.mojom-blink.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace base {
class TickClock;
}

namespace blink {

class Agent;
class Document;
class TimerBase;

// This class tracks agent-related metrics for reporting in TRACE and UMA
// metrics. It listens for documents being attached/detached to an execution
// context and tracks which agent these documents are associated with.
//
// We report metrics periodically to track how long we spent in any given state.
// For example, suppose that for 10 seconds a page had just one agent, then an
// ad frame loads causing a document to load with a second agent. After 5
// seconds, the user closes the browser. In this case, we report:
//
// Histogram
// 1 ----------O        10
// 2 -----O             5
//
// We therefore keep track of how much time has elapsed since the previous
// report. Metrics are reported whenever a document is added or removed, as
// well as at a regular interval.
//
// This class is based on the metrics tracked in:
// chrome/browser/performance_manager/observers/isolation_context_metrics.cc
// It should eventually be migrated to that place.
class AgentMetricsCollector final
    : public GarbageCollected<AgentMetricsCollector> {
 public:
  AgentMetricsCollector();
  ~AgentMetricsCollector();

  void DidAttachDocument(const Document&);
  void DidDetachDocument(const Document&);

  void ReportMetrics();

  void SetTickClockForTesting(const base::TickClock* clock) { clock_ = clock; }

  void Trace(blink::Visitor*);

 private:
  void AddTimeToTotalAgents(int time_delta_to_add);
  void ReportToBrowser();

  void ReportingTimerFired(TimerBase*);

  mojo::Remote<blink::mojom::blink::AgentMetricsCollectorHost>&
  GetAgentMetricsCollectorHost();

  std::unique_ptr<TaskRunnerTimer<AgentMetricsCollector>> reporting_timer_;
  base::TimeTicks time_last_reported_;

  // Keep a map from each agent to all the documents associated with that
  // agent. When the last document from the set is removed, we delete the key
  // from the map.
  using DocumentSet = HeapHashSet<WeakMember<const Document>>;
  using AgentToDocumentsMap =
      HeapHashMap<WeakMember<Agent>, Member<DocumentSet>>;
  AgentToDocumentsMap agent_to_documents_map_;

  const base::TickClock* clock_;

  mojo::Remote<blink::mojom::blink::AgentMetricsCollectorHost>
      agent_metrics_collector_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_METRICS_COLLECTOR_H_
