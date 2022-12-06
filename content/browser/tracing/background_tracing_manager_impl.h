// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_tracing_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/mojom/background_tracing_agent.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace tracing {
namespace mojom {
class BackgroundTracingAgent;
class BackgroundTracingAgentProvider;
}  // namespace mojom
}  // namespace tracing

namespace content {
namespace mojom {
class ChildProcess;
}  // namespace mojom

class BackgroundTracingRule;
class BackgroundTracingActiveScenario;
class TracingDelegate;

class BackgroundTracingManagerImpl : public BackgroundTracingManager {
 public:
  // Enabled state observers get a callback when the state of background tracing
  // changes.
  class CONTENT_EXPORT EnabledStateObserver {
   public:
    // Called when the activation of a background tracing scenario is
    // successful.
    virtual void OnScenarioActivated(
        const BackgroundTracingConfigImpl* config) = 0;

    // In case the scenario was aborted before or after tracing was enabled.
    virtual void OnScenarioAborted() = 0;

    // Called after tracing is enabled on all processes because the rule was
    // triggered.
    virtual void OnTracingEnabled() = 0;

    virtual ~EnabledStateObserver() = default;
  };

  class AgentObserver {
   public:
    virtual void OnAgentAdded(
        tracing::mojom::BackgroundTracingAgent* agent) = 0;
    virtual void OnAgentRemoved(
        tracing::mojom::BackgroundTracingAgent* agent) = 0;
  };

  // These values are used for a histogram. Do not reorder.
  enum class Metrics {
    SCENARIO_ACTIVATION_REQUESTED = 0,
    SCENARIO_ACTIVATED_SUCCESSFULLY = 1,
    RECORDING_ENABLED = 2,
    PREEMPTIVE_TRIGGERED = 3,
    REACTIVE_TRIGGERED = 4,
    FINALIZATION_ALLOWED = 5,
    FINALIZATION_DISALLOWED = 6,
    FINALIZATION_STARTED = 7,
    OBSOLETE_FINALIZATION_COMPLETE = 8,
    SCENARIO_ACTION_FAILED_LOWRES_CLOCK = 9,
    UPLOAD_FAILED = 10,
    UPLOAD_SUCCEEDED = 11,
    STARTUP_SCENARIO_TRIGGERED = 12,
    LARGE_UPLOAD_WAITING_TO_RETRY = 13,
    SYSTEM_TRIGGERED = 14,
    REACHED_CODE_SCENARIO_TRIGGERED = 15,
    FINALIZATION_STARTED_WITH_LOCAL_OUTPUT = 16,
    NUMBER_OF_BACKGROUND_TRACING_METRICS,
  };
  static void RecordMetric(Metrics metric);

  CONTENT_EXPORT static BackgroundTracingManagerImpl& GetInstance();

  BackgroundTracingManagerImpl(const BackgroundTracingManagerImpl&) = delete;
  BackgroundTracingManagerImpl& operator=(const BackgroundTracingManagerImpl&) =
      delete;

  // Callable from any thread.
  static void ActivateForProcess(int child_process_id,
                                 mojom::ChildProcess* child_process);

  bool SetActiveScenario(std::unique_ptr<BackgroundTracingConfig>,
                         DataFiltering data_filtering) override;
  bool SetActiveScenarioWithReceiveCallback(
      std::unique_ptr<BackgroundTracingConfig>,
      ReceiveCallback receive_callback,
      DataFiltering data_filtering) override;
  void AbortScenario();
  bool HasActiveScenario() override;

  // Named triggers
  void TriggerNamedEvent(TriggerHandle, StartedFinalizingCallback) override;
  TriggerHandle RegisterTriggerType(base::StringPiece trigger_name) override;
  const std::string& GetTriggerNameFromHandle(
      TriggerHandle trigger_handle) override;

  void OnHistogramTrigger(const std::string& histogram_name);

  void OnRuleTriggered(const BackgroundTracingRule* triggered_rule,
                       StartedFinalizingCallback callback);
  bool HasTraceToUpload() override;
  std::string GetLatestTraceToUpload() override;
  void SetTraceToUpload(std::unique_ptr<std::string> trace_data);
  std::unique_ptr<BackgroundTracingConfig> GetBackgroundTracingConfig(
      const std::string& trial_name) override;

  // Add/remove EnabledStateObserver.
  CONTENT_EXPORT void AddEnabledStateObserver(EnabledStateObserver* observer);
  CONTENT_EXPORT void RemoveEnabledStateObserver(
      EnabledStateObserver* observer);

  // Add/remove Agent{Observer}.
  void AddAgent(tracing::mojom::BackgroundTracingAgent* agent);
  void RemoveAgent(tracing::mojom::BackgroundTracingAgent* agent);
  void AddAgentObserver(AgentObserver* observer);
  void RemoveAgentObserver(AgentObserver* observer);

  void AddMetadataGeneratorFunction();

  bool IsAllowedFinalization(bool is_crash_scenario) const;

  // Called by BackgroundTracingActiveScenario
  void OnStartTracingDone();

  // For tests
  CONTENT_EXPORT BackgroundTracingActiveScenario* GetActiveScenarioForTesting();
  CONTENT_EXPORT void InvalidateTriggerHandlesForTesting();
  CONTENT_EXPORT bool IsTracingForTesting();
  void WhenIdle(IdleCallback idle_callback) override;
  CONTENT_EXPORT void AbortScenarioForTesting() override;
  CONTENT_EXPORT void SetTraceToUploadForTesting(
      std::unique_ptr<std::string> trace_data) override;
  void SetConfigTextFilterForTesting(
      ConfigTextFilterForTesting predicate) override;

 private:
  friend class base::NoDestructor<BackgroundTracingManagerImpl>;

  BackgroundTracingManagerImpl();
  ~BackgroundTracingManagerImpl() override;

  absl::optional<base::Value> GenerateMetadataDict();
  void GenerateMetadataProto(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata,
      bool privacy_filtering_enabled);
  bool IsTriggerHandleValid(TriggerHandle handle) const;
  void OnScenarioAborted();
  static void AddPendingAgent(
      int child_process_id,
      mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentProvider>
          provider);
  static void ClearPendingAgent(int child_process_id);
  void MaybeConstructPendingAgents();

  std::unique_ptr<BackgroundTracingActiveScenario> active_scenario_;

  std::unique_ptr<TracingDelegate> delegate_;
  std::map<TriggerHandle, std::string> trigger_handles_;
  int trigger_handle_ids_;

  // Note, these sets are not mutated during iteration so it is okay to not use
  // base::ObserverList.
  std::set<EnabledStateObserver*> background_tracing_observers_;
  std::set<tracing::mojom::BackgroundTracingAgent*> agents_;
  std::set<AgentObserver*> agent_observers_;

  std::map<int, mojo::Remote<tracing::mojom::BackgroundTracingAgentProvider>>
      pending_agents_;

  IdleCallback idle_callback_;

  // This field contains serialized trace log proto.
  std::string trace_to_upload_;

  // Callback to override the background tracing config for testing.
  ConfigTextFilterForTesting config_text_filter_for_testing_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_
