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

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "base/token.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/trace_report/trace_report_database.h"
#include "content/browser/tracing/trace_report/trace_upload_list.h"
#include "content/browser/tracing/tracing_scenario.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_tracing_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/mojom/background_tracing_agent.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace tracing::mojom {
class BackgroundTracingAgent;
class BackgroundTracingAgentProvider;
}  // namespace tracing::mojom

namespace content {
namespace mojom {
class ChildProcess;
}  // namespace mojom

class BackgroundTracingActiveScenario;
class TracingDelegate;

CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackgroundTracingDatabase);

class BackgroundTracingManagerImpl : public BackgroundTracingManager,
                                     public TraceUploadList,
                                     public TracingScenario::Delegate {
 public:
  class AgentObserver {
   public:
    virtual void OnAgentAdded(
        tracing::mojom::BackgroundTracingAgent* agent) = 0;
    virtual void OnAgentRemoved(
        tracing::mojom::BackgroundTracingAgent* agent) = 0;
  };

  using ScenarioCountMap = base::flat_map<std::string, size_t>;
  using FinishedProcessingCallback =
      TraceUploadList::FinishedProcessingCallback;

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
    DATABASE_INITIALIZATION_FAILED = 17,
    DATABASE_CLEANUP_FAILED = 18,
    SAVE_TRACE_FAILED = 19,
    SAVE_TRACE_SUCCEEDED = 20,
    NUMBER_OF_BACKGROUND_TRACING_METRICS,
  };
  static void RecordMetric(Metrics metric);

  CONTENT_EXPORT static BackgroundTracingManagerImpl& GetInstance();

  BackgroundTracingManagerImpl();
  ~BackgroundTracingManagerImpl() override;

  BackgroundTracingManagerImpl(const BackgroundTracingManagerImpl&) = delete;
  BackgroundTracingManagerImpl& operator=(const BackgroundTracingManagerImpl&) =
      delete;

  // Callable from any thread.
  static void ActivateForProcess(int child_process_id,
                                 mojom::ChildProcess* child_process);

  void SetReceiveCallback(ReceiveCallback receive_callback) override;
  bool InitializeScenarios(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      DataFiltering data_filtering) override;

  bool SetActiveScenario(std::unique_ptr<BackgroundTracingConfig>,
                         DataFiltering data_filtering) override;
  bool HasActiveScenario() override;
  void DeleteTracesInDateRange(base::Time start, base::Time end) override;

  // TracingScenario::Delegate:
  bool OnScenarioActive(TracingScenario* scenario) override;
  bool OnScenarioIdle(TracingScenario* scenario) override;
  void OnScenarioRecording(TracingScenario* scenario) override;
  void SaveTrace(TracingScenario* scenario,
                 const BackgroundTracingRule* triggered_rule,
                 std::string&& serialized_trace) override;

  void SetNamedTriggerCallback(const std::string& trigger_name,
                               base::RepeatingCallback<bool()> callback);

  bool HasTraceToUpload() override;
  void GetTraceToUpload(
      base::OnceCallback<void(absl::optional<std::string>,
                              absl::optional<std::string>)>) override;
  std::unique_ptr<BackgroundTracingConfig> GetBackgroundTracingConfig(
      const std::string& trial_name) override;
  void SetSystemProfileRecorder(
      base::RepeatingCallback<std::string()> recorder) override;

  CONTENT_EXPORT size_t GetScenarioSavedCount(const std::string& scenario_name);
  CONTENT_EXPORT void InitializeTraceReportDatabase(
      bool open_in_memory = false);

  // TraceUploadList
  void OpenDatabaseIfExists() override;
  void GetAllTraceReports(GetReportsCallback callback) override;
  void DeleteSingleTrace(const base::Token& trace_uuid,
                         FinishedProcessingCallback callback) override;
  void DeleteAllTraces(FinishedProcessingCallback callback) override;
  void UserUploadSingleTrace(const base::Token& trace_uuid,
                             FinishedProcessingCallback callback) override;
  void DownloadTrace(const base::Token& trace_uuid,
                     GetProtoCallback callback) override;

  // Add/remove EnabledStateTestObserver.
  CONTENT_EXPORT void AddEnabledStateObserverForTesting(
      BackgroundTracingManager::EnabledStateTestObserver* observer);
  CONTENT_EXPORT void RemoveEnabledStateObserverForTesting(
      BackgroundTracingManager::EnabledStateTestObserver* observer);

  // Add/remove Agent{Observer}.
  void AddAgent(tracing::mojom::BackgroundTracingAgent* agent);
  void RemoveAgent(tracing::mojom::BackgroundTracingAgent* agent);
  void AddAgentObserver(AgentObserver* observer);
  void RemoveAgentObserver(AgentObserver* observer);

  void AddMetadataGeneratorFunction();

  // Called by BackgroundTracingActiveScenario
  void OnStartTracingDone();
  void OnProtoDataComplete(std::string&& serialized_trace,
                           const std::string& scenario_name,
                           const std::string& rule_name,
                           bool is_crash_scenario,
                           const base::Token& uuid);

  // For tests
  CONTENT_EXPORT BackgroundTracingActiveScenario* GetActiveScenarioForTesting();
  CONTENT_EXPORT void InvalidateTriggersCallbackForTesting();
  CONTENT_EXPORT bool IsTracingForTesting();
  CONTENT_EXPORT void AbortScenarioForTesting() override;
  CONTENT_EXPORT void SaveTraceForTesting(std::string&& serialized_trace,
                                          const std::string& scenario_name,
                                          const std::string& rule_name,
                                          const base::Token& uuid) override;

 private:
#if BUILDFLAG(IS_ANDROID)
  // ~1MB compressed size.
  constexpr static int kUploadLimitKb = 5 * 1024;
#else
  // Less than 10MB compressed size.
  constexpr static int kUploadLimitKb = 30 * 1024;
#endif

  bool RequestActivateScenario();

  // Named triggers
  bool DoEmitNamedTrigger(const std::string& trigger_name) override;

  void GenerateMetadataProto(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata,
      bool privacy_filtering_enabled);
  void OnScenarioAborted();
  static void AddPendingAgent(
      int child_process_id,
      mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentProvider>
          provider);
  static void ClearPendingAgent(int child_process_id);
  void MaybeConstructPendingAgents();
  void OnFinalizeComplete(absl::optional<BaseTraceReport> trace_to_upload,
                          bool success);
  void OnTraceDatabaseCreated(ScenarioCountMap scenario_saved_counts,
                              absl::optional<BaseTraceReport> trace_to_upload,
                              bool success);
  void OnTraceDatabaseUpdated(ScenarioCountMap scenario_saved_counts);
  void OnTraceSaved(const std::string& scenario_name,
                    absl::optional<NewTraceReport> trace_to_upload,
                    bool success);
  void CleanDatabase();
  size_t GetTraceUploadLimitKb() const;

  std::unique_ptr<TracingDelegate> delegate_;
  std::unique_ptr<BackgroundTracingActiveScenario> legacy_active_scenario_;
  std::vector<std::unique_ptr<TracingScenario>> scenarios_;
  raw_ptr<TracingScenario> active_scenario_{nullptr};
  ReceiveCallback receive_callback_;
  base::RepeatingCallback<std::string()> system_profile_recorder_;

  bool requires_anonymized_data_ = true;

  std::map<std::string, base::RepeatingCallback<bool()>>
      named_trigger_callbacks_;

  // Note, these sets are not mutated during iteration so it is okay to not use
  // base::ObserverList.
  std::set<EnabledStateTestObserver*> background_tracing_observers_;
  std::set<tracing::mojom::BackgroundTracingAgent*> agents_;
  std::set<AgentObserver*> agent_observers_;

  std::map<int, mojo::Remote<tracing::mojom::BackgroundTracingAgentProvider>>
      pending_agents_;

  ScenarioCountMap scenario_saved_counts_;

  // Task runner on which |trace_database_| lives.
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  // This contains all the traces saved locally.
  std::unique_ptr<TraceReportDatabase, base::OnTaskRunnerDeleter>
      trace_database_;

  absl::optional<NewTraceReport> trace_report_to_upload_;

  // Timer to delete traces older than 2 weeks.
  base::RepeatingTimer clean_database_timer_;

  // All the upload limits below are set for uncompressed trace log. On
  // compression the data size usually reduces by 3x for size < 10MB, and the
  // compression ratio grows up to 8x if the buffer size is around 100MB.
  size_t upload_limit_network_kb_ = 1024;
  size_t upload_limit_kb_ = kUploadLimitKb;

  base::WeakPtrFactory<BackgroundTracingManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_
