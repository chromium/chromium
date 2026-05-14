// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_MANAGER_H_
#define SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_MANAGER_H_

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/token.h"
#include "base/trace_event/named_trigger.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "services/tracing/public/cpp/background_tracing/trace_report_database.h"
#include "services/tracing/public/cpp/background_tracing/trace_upload_list.h"
#include "services/tracing/public/cpp/background_tracing/tracing_scenario.h"

namespace tracing {

// BackgroundTracingManager is used on the browser process to trigger the
// collection of trace data and upload the results. Only the browser UI thread
// is allowed to interact with the BackgroundTracingManager. All callbacks are
// called on the UI thread.
class COMPONENT_EXPORT(BACKGROUND_TRACING_CPP) BackgroundTracingManager
    : public base::trace_event::NamedTriggerManager,
      public TraceUploadList,
      public TracingScenario::Delegate {
 public:
  enum DataFiltering {
    NO_DATA_FILTERING,
    ANONYMIZE_DATA,
    ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME,
  };

  using ScenarioCountMap = base::flat_map<std::string, size_t>;

  // If a ReceiveCallback is set it will be called on the UI thread every time
  // the BackgroundTracingManager finalizes a trace. The first parameter of
  // this callback is the trace data. The second is a callback to notify the
  // BackgroundTracingManager that you've finished processing the trace data
  // and whether we were successful or not.
  //
  // Example:
  //
  // void Upload(std::string data,
  //             FinishedProcessingCallback done_callback) {
  //   base::PostTaskAndReply(
  //       FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
  //       base::BindOnce(&DoUploadInBackground, std::move(data)),
  //       std::move(done_callback));
  // }
  //
  using FinishedProcessingCallback =
      TraceUploadList::FinishedProcessingCallback;

  // These values are used for a histogram. Do not reorder.
  enum class Metrics {
    SCENARIO_ACTIVATION_REQUESTED = 0,
    SCENARIO_ACTIVATED_SUCCESSFULLY = 1,
    // RECORDING_ENABLED = 2, Obsolete
    // PREEMPTIVE_TRIGGERED = 3, Obsolete
    // REACTIVE_TRIGGERED = 4, Obsolete
    // FINALIZATION_ALLOWED = 5, Obsolete
    // FINALIZATION_DISALLOWED = 6, Obsolete
    FINALIZATION_STARTED = 7,
    // OBSOLETE_FINALIZATION_COMPLETE = 8, Obsolete
    SCENARIO_ACTION_FAILED_LOWRES_CLOCK = 9,
    UPLOAD_FAILED = 10,
    UPLOAD_SUCCEEDED = 11,
    // STARTUP_SCENARIO_TRIGGERED = 12, Obsolete
    LARGE_UPLOAD_WAITING_TO_RETRY = 13,
    // SYSTEM_TRIGGERED = 14, Obsolete
    // REACHED_CODE_SCENARIO_TRIGGERED = 15, Obsolete
    FINALIZATION_STARTED_WITH_LOCAL_OUTPUT = 16,
    DATABASE_INITIALIZATION_FAILED = 17,
    DATABASE_CLEANUP_FAILED = 18,
    SAVE_TRACE_FAILED = 19,
    SAVE_TRACE_SUCCEEDED = 20,
    NUMBER_OF_BACKGROUND_TRACING_METRICS,
  };
  static void RecordMetric(Metrics metric);

  // Enabled state observers get a callback when the state of background tracing
  // changes.
  class EnabledStateTestObserver {
   public:
    // Called when |scenario_name| becomes active.
    virtual void OnScenarioActive(const std::string& scenario_name) {}
    // Called when |scenario_name| becomes idle again.
    virtual void OnScenarioIdle(const std::string& scenario_name) {}
    // Called when tracing is enabled on all processes because of an active
    // scenario.
    virtual void OnTraceStarted() {}
    // Called when tracing stopped and |proto_content| was received.
    virtual void OnTraceReceived(const std::string& proto_content) {}
    // Called when the trace content is saved.
    virtual void OnTraceSaved() {}

   protected:
    ~EnabledStateTestObserver() = default;
  };

  void AddEnabledStateObserverForTesting(EnabledStateTestObserver* observer);
  void RemoveEnabledStateObserverForTesting(EnabledStateTestObserver* observer);

  static BackgroundTracingManager& GetInstance();

  using ReceiveCallback = base::RepeatingCallback<void(
      const std::string& file_name,
      std::string file_contents,
      base::OnceCallback<void(bool)> done_callback)>;

  void SetReceiveCallback(ReceiveCallback receive_callback);

  // Initializes a list of triggers from `config` to be forwarded to
  // perfetto. This is useful when system tracing is running. This will
  // fail and return false if any scenario was previously enabled,
  // either with InitializeFieldScenarios() or SetEnabledScenarios().
  // This shouldn't be called if SetActiveScenario() was previously
  // called.
  bool InitializePerfettoTriggerRules(
      const perfetto::protos::gen::TracingTriggerRulesConfig& config);

  // Tracing Scenarios are enrolled by clients based on a set of start and
  // stop rules that delimitate a meaningful tracing interval, usually covering
  // a user journey or a guardian metric (e.g. FirstContentfulPaint). This can
  // only be called once.
  // Field scenarios are enabled automatically for a subset of the population.
  // Preset scenarios are predefined and enabled locally by manual action from
  // a user. When enabled, they take precedence over field scenarios if any.

  // Saves and enables a set of field scenarios, each associated with specific
  // tracing configs. Returns true if all scenarios were successfully
  // initialized. This will fail and return false if any scenario was previously
  // enabled, either with InitializeFieldScenarios() or SetEnabledScenarios().
  // `force_uploads` allows scenario to ignore upload quotas, and
  // `upload_limit_kb` overrides default upload size limits if not 0. This
  // shouldn't be called if SetActiveScenario() was previously called.
  bool InitializeFieldScenarios(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      DataFiltering data_filtering,
      bool force_upload,
      size_t upload_limit_kb);

  // Saves a set of preset scenarios, each associated with specific tracing
  // configs, without enabling them. These scenarios can be enabled with
  // SetEnabledScenarios(). Returns the list of scenario hashes that were
  // successfully added. This can be called multiple times.
  std::vector<std::string> AddPresetScenarios(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      DataFiltering data_filtering);

  // Enables a list of preset scenarios identified by their hashes. This
  // disables all previously enabled scenarios and aborts the current background
  // tracing session if any. Since InitializeFieldScenarios() above fails if
  // scenarios are enabled, field scenarios can't be re-enabled after calling
  // this.
  bool SetEnabledScenarios(std::vector<std::string> enabled_scenarios_hashes);

  bool HasActiveScenario();
  void DeleteTracesInDateRange(base::Time start, base::Time end);

  // TracingScenario::Delegate implementation:
  bool OnScenarioActive(TracingScenario* scenario) override;
  bool OnScenarioIdle(TracingScenario* scenario) override;
  void OnScenarioError(TracingScenario* scenario,
                       perfetto::TracingError error) override;
  bool OnScenarioCloned(TracingScenario* scenario) override;
  void OnScenarioRecording(TracingScenario* scenario) override;
  void SaveTrace(TracingScenario* scenario,
                 base::Token trace_uuid,
                 const BackgroundTracingRule* triggered_rule,
                 std::string&& serialized_trace) override;

  std::vector<std::string> OverwritePresetScenarios(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      DataFiltering data_filtering);

  // Returns the list of scenario hashes that are currently enabled. These are
  // either all preset scenarios or all field scenarios.
  std::vector<std::string> GetEnabledScenarios() const;

  // Returns true whether a trace is ready to be uploaded.
  bool HasTraceToUpload();

  // Loads the content of the next trace saved for uploading and returns it
  // through `callback` in a gzip of a serialized proto of message type
  // perfetto::Trace. `callback` may be invoked either synchronously or on a
  // thread pool task runner. Iff `compressed_trace_content` is valid,
  // `upload_complete_closure` should be invoked on any task runner once the
  // trace is uploaded successfully.
  void GetTraceToUpload(
      base::OnceCallback<void(std::optional<std::string>,
                              std::optional<std::string>,
                              base::OnceClosure)> receive_callback);

  size_t GetScenarioSavedCount(const std::string& scenario_name);
  void InitializeTraceReportDatabase(bool open_in_memory = false);

  explicit BackgroundTracingManager();

  ~BackgroundTracingManager() override;

  BackgroundTracingManager(const BackgroundTracingManager&) = delete;
  BackgroundTracingManager& operator=(const BackgroundTracingManager&) = delete;

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
  void OnStartTracingDone();
  void OnProtoDataComplete(std::string&& serialized_trace,
                           const std::string& scenario_name,
                           const std::string& rule_name,
                           std::optional<int32_t> rule_value,
                           bool privacy_filter_enabled,
                           bool is_local_scenario,
                           bool force_upload,
                           const base::Token& uuid);

  // For tests
  void InvalidateTriggersCallbackForTesting();
  bool IsTracingForTesting();
  void AbortScenarioForTesting();
  void SaveTraceForTesting(std::string&& serialized_trace,
                           const std::string& scenario_name,
                           const std::string& rule_name,
                           const base::Token& uuid);
  void SetUploadLimitsForTesting(size_t upload_limit_kb,
                                 size_t upload_limit_network_kb);

  void GenerateMetadataProto(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata,
      bool privacy_filtering_enabled);

 protected:
  virtual bool GetBackgroundStartupTracingEnabled() const;
  virtual bool IsRecordingAllowed(bool privacy_filter_enabled,
                                  base::TimeTicks scenario_start_time) = 0;
  virtual bool ShouldSaveUnuploadedTrace() = 0;
  virtual std::string RecordSerializedSystemProfileMetrics() = 0;
  virtual std::optional<base::FilePath> GetLocalTracesDirectory() = 0;

#if BUILDFLAG(IS_ANDROID)
  // ~1MB compressed size.
  constexpr static int kDefaultUploadLimitKb = 5 * 1024;
#else
  // Less than 10MB compressed size.
  constexpr static int kDefaultUploadLimitKb = 30 * 1024;
#endif

  virtual bool RequestActivateScenario();
  void DisableScenarios();
  void AddMetadataGeneratorFunction();
  std::vector<std::string> AddPresetScenariosImpl(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      DataFiltering data_filtering,
      bool overwrite_conflicts);

  // Named triggers
  bool DoEmitNamedTrigger(const std::string& trigger_name,
                          std::optional<int32_t> value,
                          uint64_t flow_id) override;

  void OnScenarioAborted();
  virtual void MaybeConstructPendingAgents() = 0;
  void OnFinalizeComplete(
      std::optional<tracing::BaseTraceReport> trace_to_upload,
      bool success);
  void OnTraceDatabaseCreated(
      ScenarioCountMap scenario_saved_counts,
      std::optional<tracing::BaseTraceReport> trace_to_upload,
      bool success);
  void OnTraceDatabaseUpdated(ScenarioCountMap scenario_saved_counts);
  void OnTraceSaved(const std::string& scenario_name,
                    std::optional<tracing::BaseTraceReport> trace_to_upload,
                    bool success);
  void CleanDatabase();

  std::vector<std::unique_ptr<TracingScenario>> field_scenarios_;
  base::flat_map<std::string, std::unique_ptr<TracingScenario>>
      preset_scenarios_;
  std::vector<raw_ptr<TracingScenario>> enabled_scenarios_;
  raw_ptr<TracingScenario> active_scenario_{nullptr};
  base::TimeTicks scenario_start_time_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> trigger_rules_;
  ReceiveCallback receive_callback_;

  // Note, these sets are not mutated during iteration so it is okay to not use
  // base::ObserverList.
  std::set<raw_ptr<EnabledStateTestObserver, SetExperimental>>
      background_tracing_observers_;

  ScenarioCountMap scenario_saved_counts_;

  // Task runner on which |trace_database_| lives.
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  // This contains all the traces saved locally.
  std::unique_ptr<tracing::TraceReportDatabase, base::OnTaskRunnerDeleter>
      trace_database_;

  std::optional<tracing::BaseTraceReport> trace_report_to_upload_;

  // Timer to delete traces older than 2 weeks.
  base::RepeatingTimer clean_database_timer_;

  // All the upload limits below are set for uncompressed trace log. On
  // compression the data size usually reduces by 3x for size < 10MB, and the
  // compression ratio grows up to 8x if the buffer size is around 100MB.
  size_t upload_limit_network_kb_ = 1024;
  size_t upload_limit_kb_ = kDefaultUploadLimitKb;
  bool force_uploads_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BackgroundTracingManager> weak_factory_{this};
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_BACKGROUND_TRACING_MANAGER_H_
