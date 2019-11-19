// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent_metrics_collector.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using WTF::HashSet;
using WTF::String;
using WTF::Vector;

namespace blink {

namespace {

const char kAgentsPerRendererByTimeHistogram[] =
    "PerformanceManager.AgentsPerRendererByTime";

base::TimeDelta kReportingInterval = base::TimeDelta::FromMinutes(5);

}  // namespace

AgentMetricsCollector::AgentMetricsCollector()
    : reporting_timer_(std::make_unique<TaskRunnerTimer<AgentMetricsCollector>>(
          // Some tests might not have a MainThreadScheduler.
          scheduler::WebThreadScheduler::MainThreadScheduler()
              ? scheduler::WebThreadScheduler::MainThreadScheduler()
                    ->DefaultTaskRunner()
              : nullptr,
          this,
          &AgentMetricsCollector::ReportingTimerFired)),
      clock_(base::DefaultTickClock::GetInstance()) {
  // From now until we call CreatedNewAgent will be reported as having 0
  // agents.
  time_last_reported_ = clock_->NowTicks();
}

AgentMetricsCollector::~AgentMetricsCollector() {
  // Note: This won't be called during a fast-shutdown (i.e. tab closed). We
  // manually call it from Page::WillBeDestroyed().
  ReportMetrics();
}

void AgentMetricsCollector::DidAttachDocument(const Document& doc) {
  ReportMetrics();

  AgentToDocumentsMap::AddResult result =
      agent_to_documents_map_.insert(doc.GetAgent(), nullptr);
  if (result.is_new_entry)
    result.stored_value->value = MakeGarbageCollected<DocumentSet>();

  result.stored_value->value->insert(&doc);

  ReportToBrowser();
}

void AgentMetricsCollector::DidDetachDocument(const Document& doc) {
  ReportMetrics();

  auto agent_itr = agent_to_documents_map_.find(doc.GetAgent());
  DCHECK(agent_itr != agent_to_documents_map_.end());

  DocumentSet& documents = *agent_itr->value.Get();
  auto document_itr = documents.find(&doc);
  DCHECK(document_itr != documents.end());

  documents.erase(document_itr);

  if (documents.IsEmpty())
    agent_to_documents_map_.erase(agent_itr);

  ReportToBrowser();
}

void AgentMetricsCollector::ReportMetrics() {
  DCHECK(!time_last_reported_.is_null());

  // Don't run the timer in tests. Doing so causes tests that RunUntilIdle to
  // never exit.
  if (!reporting_timer_->IsActive() && !WebTestSupport::IsRunningWebTest()) {
    reporting_timer_->StartRepeating(kReportingInterval, FROM_HERE);
  }

  // This computation and reporting is based on the one in
  // chrome/browser/performance_manager/observers/isolation_context_metrics.cc.
  base::TimeTicks now = clock_->NowTicks();

  base::TimeDelta elapsed = now - time_last_reported_;
  time_last_reported_ = now;

  // Account for edge cases like hibernate/sleep. See
  // GetSecondsSinceLastReportAndUpdate in isolation_context_metrics.cc
  if (elapsed >= 2 * kReportingInterval)
    elapsed = base::TimeDelta();

  int to_add = static_cast<int>(std::round(elapsed.InSecondsF()));

  // Time can be negative in tests when we replace the clock_.
  if (to_add <= 0)
    return;

  AddTimeToTotalAgents(to_add);
}

void AgentMetricsCollector::AddTimeToTotalAgents(int time_delta_to_add) {
  DEFINE_STATIC_LOCAL(LinearHistogram, agents_per_renderer_histogram,
                      (kAgentsPerRendererByTimeHistogram, 1, 100, 101));
  agents_per_renderer_histogram.CountMany(agent_to_documents_map_.size(),
                                          time_delta_to_add);
}

void AgentMetricsCollector::ReportToBrowser() {
  Vector<String> agents;
  for (const auto& kv : agent_to_documents_map_) {
    const Member<DocumentSet>& doc_set = kv.value;

    String tuple_origin;
    DCHECK(!doc_set->IsEmpty());
    const auto& doc = *doc_set->begin();
    auto* security_origin = doc->GetSecurityOrigin();
    if (security_origin && !security_origin->IsOpaque() &&
        !security_origin->IsLocal()) {
      // We shouldn't ever host multiple tuple-origins in an Agent. However,
      // this does happen in tests because we have
      // GetAllowUniversalAccessFromFileURLs enabled but that's ok in tests.
      tuple_origin = security_origin->Protocol() + "://" +
                     security_origin->RegistrableDomain();
    } else {
      // We use an empty string to specify that there isn't any one
      // tuple-origin this agent represents. This will typically be for
      // file:// or opaque origins. We shouldn't ever host multiple sites
      // inside an agent.
      tuple_origin = "";
    }

    agents.push_back(tuple_origin);
  }

  mojom::blink::AgentMetricsDataPtr data =
      mojom::blink::AgentMetricsData::New();
  data->agents = agents;

  GetAgentMetricsCollectorHost()->ReportRendererMetrics(std::move(data));
}

void AgentMetricsCollector::ReportingTimerFired(TimerBase*) {
  ReportMetrics();
  ReportToBrowser();
}

mojo::Remote<blink::mojom::blink::AgentMetricsCollectorHost>&
AgentMetricsCollector::GetAgentMetricsCollectorHost() {
  if (!agent_metrics_collector_host_) {
    blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        agent_metrics_collector_host_.BindNewPipeAndPassReceiver());
  }
  return agent_metrics_collector_host_;
}

void AgentMetricsCollector::Trace(blink::Visitor* visitor) {
  visitor->Trace(agent_to_documents_map_);
}

}  // namespace blink
