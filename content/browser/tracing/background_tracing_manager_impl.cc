// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_manager_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/tracing/background_startup_tracing_observer.h"
#include "content/browser/tracing/background_tracing_active_scenario.h"
#include "content/browser/tracing/background_tracing_agent_client_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/browser/tracing/trace_report/trace_report_database.h"
#include "content/browser/tracing/trace_report/trace_upload_list.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/common/child_process.mojom.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/network_change_notifier.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/zlib/google/compression_utils.h"

namespace content {

namespace {
// The time to live of a trace is currently 14 days.
const base::TimeDelta kTraceTimeToLive = base::Days(14);
// We limit uploads of 1 trace per scenario over a period of 7 days. Since
// traces live in the database for longer than 7 days, their TTL doesn't affect
// this unless the database is manually cleared.
const base::TimeDelta kMinTimeUntilNextUpload = base::Days(7);
// We limit the overall number of traces per scenario saved to the database at
// 20. When traces are deleted after their TTL, it leaves more capacity for new
// traces.
const size_t kMaxTracesPerScenario = 20;

const char kBackgroundTracingConfig[] = "config";

// |g_background_tracing_manager| is intentionally leaked on shutdown.
BackgroundTracingManager* g_background_tracing_manager = nullptr;
BackgroundTracingManagerImpl* g_background_tracing_manager_impl = nullptr;

void OpenDatabaseOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    absl::optional<base::FilePath> database_dir,
    base::OnceCallback<void(BackgroundTracingManagerImpl::ScenarioCountMap,
                            absl::optional<BaseTraceReport>,
                            bool)> on_database_created) {
  if (database->is_initialized()) {
    return;
  }
  bool success;
  if (!database_dir) {
    success = database->OpenDatabaseInMemoryForTesting();  // IN-TEST
  } else {
    success = database->OpenDatabase(*database_dir);
  }
  absl::optional<NewTraceReport> report_to_upload;
  if (base::FeatureList::IsEnabled(kBackgroundTracingDatabase)) {
    report_to_upload = database->GetNextReportPendingUpload();
  } else {
    // Traces pending upload from previous sessions have timed out.
    database->AllPendingUploadSkipped(SkipUploadReason::kUploadTimedOut);
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_database_created),
                                database->GetScenarioCounts(),
                                std::move(report_to_upload), success));
}

void AddTraceOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    std::string&& serialized_trace,
    std::string&& serialized_system_profile,
    BaseTraceReport base_report,
    bool should_save_trace,
    bool is_crash_scenario,
    base::OnceCallback<void(absl::optional<NewTraceReport>, bool)>
        on_trace_saved) {
  if (!database->is_initialized()) {
    return;
  }
  base::Time since = base::Time::Now() - kMinTimeUntilNextUpload;
  auto upload_count =
      database->UploadCountSince(base_report.scenario_name, since);
  if (base_report.skip_reason == SkipUploadReason::kNoSkip &&
      !is_crash_scenario && upload_count && *upload_count > 0) {
    base_report.skip_reason = SkipUploadReason::kScenarioQuotaExceeded;
    if (!should_save_trace) {
      return;
    }
  }

  std::string compressed_trace;
  bool success = compression::GzipCompress(serialized_trace, &compressed_trace);
  absl::optional<NewTraceReport> report_to_upload;
  if (success) {
    UMA_HISTOGRAM_COUNTS_100000("Tracing.Background.CompressedTraceSizeInKB",
                                compressed_trace.size() / 1024);

    if (base::FeatureList::IsEnabled(kBackgroundTracingDatabase)) {
      NewTraceReport trace_report = base_report;
      trace_report.trace_content = std::move(compressed_trace);
      trace_report.system_profile = std::move(serialized_system_profile);
      success = database->AddTrace(trace_report);
    } else {
      // When the database is disabled, we still store the base report without
      // the trace content proto to enable tracking of trace upload limits.
      success = database->AddTrace(base_report);
      if (success && base_report.skip_reason == SkipUploadReason::kNoSkip) {
        report_to_upload = std::move(base_report);
        report_to_upload->trace_content = std::move(compressed_trace);
        report_to_upload->system_profile = std::move(serialized_system_profile);
      }
    }
  }
  if (base::FeatureList::IsEnabled(kBackgroundTracingDatabase)) {
    report_to_upload = database->GetNextReportPendingUpload();
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_trace_saved),
                                std::move(report_to_upload), success));
}

void GetProtoValueOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    base::Token uuid,
    base::OnceCallback<void(absl::optional<std::string>,
                            absl::optional<std::string>)> receive_callback,
    base::OnceCallback<void(absl::optional<BaseTraceReport>, bool)>
        on_finalize_complete) {
  DCHECK(base::FeatureList::IsEnabled(kBackgroundTracingDatabase));
  auto trace_content = database->GetTraceContent(uuid);
  auto serialized_system_profile = database->GetSystemProfile(uuid);
  absl::optional<ClientTraceReport> next_report;
  if (trace_content) {
    if (database->UploadComplete(uuid, base::Time::Now())) {
      next_report = database->GetNextReportPendingUpload();
    }
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(on_finalize_complete), std::move(next_report),
                     trace_content.has_value()));
  std::move(receive_callback)
      .Run(std::move(trace_content), std::move(serialized_system_profile));
}

}  // namespace

BASE_FEATURE(kBackgroundTracingDatabase,
             "BackgroundTracingDatabase",
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
const char BackgroundTracingManager::kContentTriggerConfig[] =
    "content-trigger-config";

// static
std::unique_ptr<BackgroundTracingManager>
BackgroundTracingManager::CreateInstance() {
  return std::make_unique<BackgroundTracingManagerImpl>();
}

// static
BackgroundTracingManager& BackgroundTracingManager::GetInstance() {
  CHECK_NE(nullptr, g_background_tracing_manager);
  return *g_background_tracing_manager;
}

// static
void BackgroundTracingManager::SetInstance(
    BackgroundTracingManager* tracing_manager) {
  DCHECK(g_background_tracing_manager == nullptr || tracing_manager == nullptr);
  g_background_tracing_manager = tracing_manager;
}

// static
bool BackgroundTracingManager::EmitNamedTrigger(
    const std::string& trigger_name) {
  if (g_background_tracing_manager) {
    return g_background_tracing_manager->DoEmitNamedTrigger(trigger_name);
  }
  return false;
}

// static
void BackgroundTracingManagerImpl::RecordMetric(Metrics metric) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.ScenarioState", metric,
                            Metrics::NUMBER_OF_BACKGROUND_TRACING_METRICS);
}

// static
BackgroundTracingManagerImpl& BackgroundTracingManagerImpl::GetInstance() {
  CHECK_NE(nullptr, g_background_tracing_manager_impl);
  return *g_background_tracing_manager_impl;
}

// static
void BackgroundTracingManagerImpl::ActivateForProcess(
    int child_process_id,
    mojom::ChildProcess* child_process) {
  // NOTE: Called from any thread.

  mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentProvider>
      pending_provider;
  child_process->GetBackgroundTracingAgentProvider(
      pending_provider.InitWithNewPipeAndPassReceiver());

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundTracingManagerImpl::AddPendingAgent,
                                child_process_id, std::move(pending_provider)));
}

BackgroundTracingManagerImpl::BackgroundTracingManagerImpl()
    : delegate_(GetContentClient()->browser()->GetTracingDelegate()),
      database_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      trace_database_(nullptr,
                      base::OnTaskRunnerDeleter(database_task_runner_)) {
  SetInstance(this);
  g_background_tracing_manager_impl = this;
  BackgroundStartupTracingObserver::GetInstance();
}

BackgroundTracingManagerImpl::~BackgroundTracingManagerImpl() {
  DCHECK_EQ(this, g_background_tracing_manager_impl);
  if (active_scenario_) {
    active_scenario_->Abort();
  } else {
    for (auto& scenario : scenarios_) {
      scenario->Disable();
    }
  }
  if (legacy_active_scenario_) {
    legacy_active_scenario_->AbortScenario();
  }
  SetInstance(nullptr);
  g_background_tracing_manager_impl = nullptr;
}

void BackgroundTracingManagerImpl::OpenDatabaseIfExists() {
  if (trace_database_) {
    return;
  }
  absl::optional<base::FilePath> database_dir =
      GetContentClient()->browser()->GetLocalTracesDirectory();
  if (!database_dir.has_value()) {
    return;
  }
  trace_database_ = {new TraceReportDatabase,
                     base::OnTaskRunnerDeleter(database_task_runner_)};
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](TraceReportDatabase* trace_database, base::FilePath path) {
            trace_database->OpenDatabaseIfExists(path);
          },
          base::Unretained(trace_database_.get()), database_dir.value()));
}

void BackgroundTracingManagerImpl::GetAllTraceReports(
    GetReportsCallback callback) {
  if (!trace_database_) {
    std::move(callback).Run({});
    return;
  }

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TraceReportDatabase::GetAllReports,
                     base::Unretained(trace_database_.get())),
      std::move(callback));
}

void BackgroundTracingManagerImpl::DeleteSingleTrace(
    const base::Token& trace_uuid,
    FinishedProcessingCallback callback) {
  if (!trace_database_) {
    std::move(callback).Run(false);
    return;
  }

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TraceReportDatabase::DeleteTrace,
                     base::Unretained(trace_database_.get()), trace_uuid),
      std::move(callback));
}

void BackgroundTracingManagerImpl::DeleteAllTraces(
    TraceUploadList::FinishedProcessingCallback callback) {
  if (!trace_database_) {
    std::move(callback).Run(false);
    return;
  }

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TraceReportDatabase::DeleteAllTraces,
                     base::Unretained(trace_database_.get())),
      std::move(callback));
}

void BackgroundTracingManagerImpl::UserUploadSingleTrace(
    const base::Token& trace_uuid,
    TraceUploadList::FinishedProcessingCallback callback) {
  if (!trace_database_) {
    std::move(callback).Run(false);
    return;
  }

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TraceReportDatabase::UserRequestedUpload,
                     base::Unretained(trace_database_.get()), trace_uuid),
      std::move(callback));
}

void BackgroundTracingManagerImpl::DownloadTrace(const base::Token& trace_uuid,
                                                 GetProtoCallback callback) {
  if (!trace_database_) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TraceReportDatabase::GetTraceContent,
                     base::Unretained(trace_database_.get()), trace_uuid),
      base::BindOnce(
          [](GetProtoCallback callback,
             const absl::optional<std::string>& result) {
            if (result) {
              std::move(callback).Run(base::span<const char>(*result));
            } else {
              std::move(callback).Run(absl::nullopt);
            }
          },
          std::move(callback)));
}

void BackgroundTracingManagerImpl::OnTraceDatabaseCreated(
    ScenarioCountMap scenario_saved_counts,
    absl::optional<BaseTraceReport> trace_to_upload,
    bool creation_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scenario_saved_counts_ = std::move(scenario_saved_counts);
  trace_report_to_upload_ = std::move(trace_to_upload);
  if (!creation_result) {
    RecordMetric(Metrics::DATABASE_INITIALIZATION_FAILED);
    return;
  }
  clean_database_timer_.Start(
      FROM_HERE, base::Days(1),
      base::BindRepeating(&BackgroundTracingManagerImpl::CleanDatabase,
                          weak_factory_.GetWeakPtr()));
}

void BackgroundTracingManagerImpl::OnTraceDatabaseUpdated(
    ScenarioCountMap scenario_saved_counts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scenario_saved_counts_ = std::move(scenario_saved_counts);
}

void BackgroundTracingManagerImpl::OnTraceSaved(
    const std::string& scenario_name,
    absl::optional<NewTraceReport> trace_to_upload,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordMetric(success ? Metrics::SAVE_TRACE_SUCCEEDED
                       : Metrics::SAVE_TRACE_FAILED);
  trace_report_to_upload_ = std::move(trace_to_upload);
  if (success) {
    ++scenario_saved_counts_[scenario_name];
  }
  for (auto* observer : background_tracing_observers_) {
    observer->OnTraceSaved();
  }
}

void BackgroundTracingManagerImpl::AddMetadataGeneratorFunction() {
  auto* metadata_source = tracing::TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(
      base::BindRepeating(&BackgroundTracingManagerImpl::GenerateMetadataProto,
                          base::Unretained(this)));
}

bool BackgroundTracingManagerImpl::RequestActivateScenario() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordMetric(Metrics::SCENARIO_ACTIVATION_REQUESTED);
  // Multi-scenarios sessions can't be initialized twice.
  DCHECK(scenarios_.empty());

  if (legacy_active_scenario_ &&
      (legacy_active_scenario_->state() !=
       BackgroundTracingActiveScenario::State::kIdle)) {
    return false;
  }

  // If we don't have a high resolution timer available, traces will be
  // too inaccurate to be useful.
  if (!base::TimeTicks::IsHighResolution()) {
    RecordMetric(Metrics::SCENARIO_ACTION_FAILED_LOWRES_CLOCK);
    return false;
  }
  return true;
}

void BackgroundTracingManagerImpl::SetReceiveCallback(
    ReceiveCallback receive_callback) {
  receive_callback_ = std::move(receive_callback);
}

bool BackgroundTracingManagerImpl::InitializeScenarios(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config,
    DataFiltering data_filtering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RequestActivateScenario()) {
    return false;
  }

  requires_anonymized_data_ = (data_filtering != NO_DATA_FILTERING);
  bool enable_package_name_filter =
      (data_filtering == ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME);
  InitializeTraceReportDatabase();

  for (const auto& scenario_config : config.scenarios()) {
    auto scenario =
        TracingScenario::Create(scenario_config, requires_anonymized_data_,
                                enable_package_name_filter, this);
    if (!scenario) {
      return false;
    }
    scenarios_.push_back(std::move(scenario));
    scenarios_.back()->Enable();
  }
  RecordMetric(Metrics::SCENARIO_ACTIVATED_SUCCESSFULLY);
  return true;
}

bool BackgroundTracingManagerImpl::SetActiveScenario(
    std::unique_ptr<BackgroundTracingConfig> config,
    DataFiltering data_filtering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<BackgroundTracingConfigImpl> config_impl(
      static_cast<BackgroundTracingConfigImpl*>(config.release()));
  config_impl = BackgroundStartupTracingObserver::GetInstance()
                    .IncludeStartupConfigIfNeeded(std::move(config_impl));
  bool startup_tracing_enabled = BackgroundStartupTracingObserver::GetInstance()
                                     .enabled_in_current_session();
  if (startup_tracing_enabled) {
    // Anonymize data for startup tracing by default. We currently do not
    // support storing the config in preferences for next session.
    data_filtering = DataFiltering::ANONYMIZE_DATA;
  }
  if (!config_impl) {
    return false;
  }

  if (!RequestActivateScenario()) {
    return false;
  }

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // If startup config was not set and we're not a SYSTEM scenario (system
  // might already have started a trace in the background) but tracing was
  // enabled, then do not set any scenario.
  if (base::trace_event::TraceLog::GetInstance()->IsEnabled() &&
      !startup_tracing_enabled &&
      config_impl->tracing_mode() != BackgroundTracingConfigImpl::SYSTEM) {
    return false;
  }
#endif

  if (config_impl->upload_limit_kb()) {
    upload_limit_kb_ = *config_impl->upload_limit_kb();
  }
  if (config_impl->upload_limit_network_kb()) {
    upload_limit_network_kb_ = *config_impl->upload_limit_network_kb();
  }

  requires_anonymized_data_ = (data_filtering != NO_DATA_FILTERING);
  config_impl->set_requires_anonymized_data(requires_anonymized_data_);

  bool enable_package_name_filter =
      (data_filtering == ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME);
  config_impl->SetPackageNameFilteringEnabled(enable_package_name_filter);

  // TODO(oysteine): Retry when time_until_allowed has elapsed.
  if (delegate_ &&
      !delegate_->OnBackgroundTracingActive(requires_anonymized_data_)) {
    return false;
  }

  legacy_active_scenario_ = std::make_unique<BackgroundTracingActiveScenario>(
      std::move(config_impl), delegate_.get(),
      base::BindOnce(&BackgroundTracingManagerImpl::OnScenarioAborted,
                     base::Unretained(this)));
  for (auto* observer : background_tracing_observers_) {
    observer->OnScenarioActive(
        legacy_active_scenario_->GetConfig()->scenario_name());
  }

  InitializeTraceReportDatabase();

  if (startup_tracing_enabled) {
    RecordMetric(Metrics::STARTUP_SCENARIO_TRIGGERED);
    DoEmitNamedTrigger(kStartupTracingTriggerName);
  }

  legacy_active_scenario_->StartTracingIfConfigNeedsIt();
  RecordMetric(Metrics::SCENARIO_ACTIVATED_SUCCESSFULLY);

  return true;
}

void BackgroundTracingManagerImpl::InitializeTraceReportDatabase(
    bool open_in_memory) {
  absl::optional<base::FilePath> database_dir;
  if (!trace_database_) {
    trace_database_ = {new TraceReportDatabase,
                       base::OnTaskRunnerDeleter(database_task_runner_)};
    if (!open_in_memory) {
      database_dir = GetContentClient()->browser()->GetLocalTracesDirectory();
      if (!database_dir.has_value()) {
        OnTraceDatabaseCreated({}, absl::nullopt, false);
        return;
      }
    }
  }
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          OpenDatabaseOnDatabaseTaskRunner,
          base::Unretained(trace_database_.get()), std::move(database_dir),
          base::BindOnce(&BackgroundTracingManagerImpl::OnTraceDatabaseCreated,
                         weak_factory_.GetWeakPtr())));
}

bool BackgroundTracingManagerImpl::OnScenarioActive(
    TracingScenario* active_scenario) {
  DCHECK_EQ(active_scenario_, nullptr);
  if (GetScenarioSavedCount(active_scenario->scenario_name()) >=
      kMaxTracesPerScenario) {
    return false;
  }
  if (delegate_ &&
      !delegate_->OnBackgroundTracingActive(requires_anonymized_data_)) {
    return false;
  }
  active_scenario_ = active_scenario;
  // TODO(crbug.com/1418116): Record scenario started metrics.
  for (auto* observer : background_tracing_observers_) {
    observer->OnScenarioActive(active_scenario_->scenario_name());
  }
  for (auto& scenario : scenarios_) {
    if (scenario.get() == active_scenario) {
      continue;
    }
    scenario->Disable();
  }
  return true;
}

bool BackgroundTracingManagerImpl::OnScenarioIdle(
    TracingScenario* idle_scenario) {
  DCHECK_EQ(active_scenario_, idle_scenario);
  active_scenario_ = nullptr;
  for (auto* observer : background_tracing_observers_) {
    observer->OnScenarioIdle(idle_scenario->scenario_name());
  }
  bool is_allowed_finalization =
      !delegate_ ||
      delegate_->OnBackgroundTracingIdle(requires_anonymized_data_);
  for (auto& scenario : scenarios_) {
    scenario->Enable();
  }
  return is_allowed_finalization;
}

void BackgroundTracingManagerImpl::OnScenarioRecording(
    TracingScenario* scenario) {
  DCHECK_EQ(active_scenario_, scenario);
  OnStartTracingDone();
}

void BackgroundTracingManagerImpl::SaveTrace(
    TracingScenario* scenario,
    const BackgroundTracingRule* triggered_rule,
    std::string&& trace_data) {
  OnProtoDataComplete(std::move(trace_data), scenario->scenario_name(),
                      triggered_rule->rule_id(), /*is_crash_scenario=*/false,
                      scenario->GetSessionID());
}

bool BackgroundTracingManagerImpl::HasActiveScenario() {
  return legacy_active_scenario_ != nullptr || active_scenario_ != nullptr;
}

bool BackgroundTracingManagerImpl::HasTraceToUpload() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Send the logs only when the trace size is within limits. If the connection
  // type changes and we have a bigger than expected trace, then the next time
  // service asks us when wifi is available, the trace will be sent. If we did
  // collect a trace that is bigger than expected, then we will end up never
  // uploading, and drop the trace. This should never happen because the trace
  // buffer limits are set appropriately.
  if (!trace_report_to_upload_) {
    return false;
  }
  if (trace_report_to_upload_->total_size <= GetTraceUploadLimitKb() * 1024) {
    return true;
  }
  RecordMetric(Metrics::LARGE_UPLOAD_WAITING_TO_RETRY);
  return false;
}

void BackgroundTracingManagerImpl::GetTraceToUpload(
    base::OnceCallback<void(absl::optional<std::string>,
                            absl::optional<std::string>)> receive_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!trace_report_to_upload_) {
    std::move(receive_callback).Run(absl::nullopt, absl::nullopt);
    return;
  }

  // Trace content was kept in memory.
  if (!trace_report_to_upload_->trace_content.empty()) {
    std::move(receive_callback)
        .Run(std::move(trace_report_to_upload_->trace_content),
             std::move(trace_report_to_upload_->system_profile));
    database_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](TraceReportDatabase* trace_database, const base::Token& uuid) {
              trace_database->UploadComplete(uuid, base::Time::Now());
            },
            base::Unretained(trace_database_.get()),
            trace_report_to_upload_->uuid));
    OnFinalizeComplete(absl::nullopt, true);
    return;
  }

  DCHECK(trace_database_);
  BaseTraceReport trace_report = *std::move(trace_report_to_upload_);
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          GetProtoValueOnDatabaseTaskRunner,
          base::Unretained(trace_database_.get()), trace_report.uuid,
          std::move(receive_callback),
          base::BindOnce(&BackgroundTracingManagerImpl::OnFinalizeComplete,
                         weak_factory_.GetWeakPtr())));
}

void BackgroundTracingManagerImpl::OnFinalizeComplete(
    absl::optional<BaseTraceReport> trace_to_upload,
    bool success) {
  trace_report_to_upload_ = std::move(trace_to_upload);
  if (success) {
    BackgroundTracingManagerImpl::RecordMetric(Metrics::UPLOAD_SUCCEEDED);
  } else {
    BackgroundTracingManagerImpl::RecordMetric(Metrics::UPLOAD_FAILED);
  }
}

void BackgroundTracingManagerImpl::AddEnabledStateObserverForTesting(
    BackgroundTracingManager::EnabledStateTestObserver* observer) {
  // Ensure that this code is called on the UI thread, except for
  // tests where a UI thread might not have been initialized at this point.
  DCHECK(
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));
  background_tracing_observers_.insert(observer);
}

void BackgroundTracingManagerImpl::RemoveEnabledStateObserverForTesting(
    BackgroundTracingManager::EnabledStateTestObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  background_tracing_observers_.erase(observer);
}

void BackgroundTracingManagerImpl::AddAgent(
    tracing::mojom::BackgroundTracingAgent* agent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agents_.insert(agent);

  for (auto* observer : agent_observers_) {
    observer->OnAgentAdded(agent);
  }
}

void BackgroundTracingManagerImpl::RemoveAgent(
    tracing::mojom::BackgroundTracingAgent* agent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto* observer : agent_observers_) {
    observer->OnAgentRemoved(agent);
  }

  agents_.erase(agent);
}

void BackgroundTracingManagerImpl::AddAgentObserver(AgentObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agent_observers_.insert(observer);

  MaybeConstructPendingAgents();

  for (auto* agent : agents_) {
    observer->OnAgentAdded(agent);
  }
}

void BackgroundTracingManagerImpl::RemoveAgentObserver(
    AgentObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agent_observers_.erase(observer);

  for (auto* agent : agents_) {
    observer->OnAgentRemoved(agent);
  }
}

BackgroundTracingActiveScenario*
BackgroundTracingManagerImpl::GetActiveScenarioForTesting() {
  DCHECK(legacy_active_scenario_);
  return legacy_active_scenario_.get();
}

bool BackgroundTracingManagerImpl::IsTracingForTesting() {
  if (legacy_active_scenario_) {
    return legacy_active_scenario_->state() ==
           BackgroundTracingActiveScenario::State::kTracing;
  } else if (active_scenario_) {
    return active_scenario_->current_state() ==
           TracingScenario::State::kRecording;
  }
  return false;
}

void BackgroundTracingManagerImpl::SaveTraceForTesting(
    std::string&& serialized_trace,
    const std::string& scenario_name,
    const std::string& rule_name,
    const base::Token& uuid) {
  InitializeTraceReportDatabase(true);
  OnProtoDataComplete(std::move(serialized_trace), scenario_name, rule_name,
                      /*is_crash_scenario=*/false, uuid);
}

size_t BackgroundTracingManagerImpl::GetScenarioSavedCount(
    const std::string& scenario_name) {
  auto it = scenario_saved_counts_.find(scenario_name);
  if (it != scenario_saved_counts_.end()) {
    return it->second;
  }
  return 0;
}

void BackgroundTracingManagerImpl::OnProtoDataComplete(
    std::string&& serialized_trace,
    const std::string& scenario_name,
    const std::string& rule_name,
    bool is_crash_scenario,
    const base::Token& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto* observer : background_tracing_observers_) {
    observer->OnTraceReceived(serialized_trace);
  }
  if (!receive_callback_) {
    DCHECK(trace_database_);

    SkipUploadReason skip_reason = SkipUploadReason::kNoSkip;
    if (!requires_anonymized_data_) {
      skip_reason = SkipUploadReason::kNotAnonymized;
    } else if (serialized_trace.size() > upload_limit_kb_ * 1024) {
      skip_reason = SkipUploadReason::kSizeLimitExceeded;
    }
    bool should_save_trace =
        !delegate_ || delegate_->ShouldSaveUnuploadedTrace();
    if (skip_reason != SkipUploadReason::kNoSkip && !should_save_trace) {
      return;
    }
    BackgroundTracingManagerImpl::RecordMetric(Metrics::FINALIZATION_STARTED);
    UMA_HISTOGRAM_COUNTS_100000("Tracing.Background.FinalizingTraceSizeInKB2",
                                serialized_trace.size() / 1024);

    BaseTraceReport base_report;
    base_report.uuid = uuid;
    base_report.creation_time = base::Time::Now();
    base_report.scenario_name = scenario_name;
    base_report.upload_rule_name = rule_name;
    base_report.total_size = serialized_trace.size();
    base_report.skip_reason = skip_reason;

    std::string serialized_system_profile;
    if (system_profile_recorder_) {
      serialized_system_profile = system_profile_recorder_.Run();
    }

    database_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            AddTraceOnDatabaseTaskRunner,
            base::Unretained(trace_database_.get()),
            std::move(serialized_trace), std::move(serialized_system_profile),
            std::move(base_report), should_save_trace, is_crash_scenario,
            base::BindOnce(&BackgroundTracingManagerImpl::OnTraceSaved,
                           weak_factory_.GetWeakPtr(), scenario_name)));
  } else {
    BackgroundTracingManagerImpl::RecordMetric(
        Metrics::FINALIZATION_STARTED_WITH_LOCAL_OUTPUT);
    receive_callback_.Run(
        std::move(serialized_trace),
        base::BindOnce(&BackgroundTracingManagerImpl::OnFinalizeComplete,
                       weak_factory_.GetWeakPtr(), absl::nullopt));
  }
}

std::unique_ptr<content::BackgroundTracingConfig>
BackgroundTracingManagerImpl::GetBackgroundTracingConfig(
    const std::string& trial_name) {
  std::string config_text =
      base::GetFieldTrialParamValue(trial_name, kBackgroundTracingConfig);
  if (config_text.empty())
    return nullptr;

  auto value = base::JSONReader::Read(config_text);
  if (!value)
    return nullptr;

  if (!value->is_dict())
    return nullptr;

  return BackgroundTracingConfig::FromDict(std::move(*value).TakeDict());
}

void BackgroundTracingManagerImpl::SetSystemProfileRecorder(
    base::RepeatingCallback<std::string()> recorder) {
  system_profile_recorder_ = std::move(recorder);
}

void BackgroundTracingManagerImpl::SetNamedTriggerCallback(
    const std::string& trigger_name,
    base::RepeatingCallback<bool()> callback) {
  if (!callback) {
    named_trigger_callbacks_.erase(trigger_name);
  } else {
    named_trigger_callbacks_.emplace(trigger_name, std::move(callback));
  }
}

bool BackgroundTracingManagerImpl::DoEmitNamedTrigger(
    const std::string& trigger_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = named_trigger_callbacks_.find(trigger_name);
  if (it == named_trigger_callbacks_.end()) {
    return false;
  }
  return it->second.Run();
}

void BackgroundTracingManagerImpl::InvalidateTriggersCallbackForTesting() {
  named_trigger_callbacks_.clear();
}

void BackgroundTracingManagerImpl::OnStartTracingDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto* observer : background_tracing_observers_) {
    observer->OnTraceStarted();
  }
}

void BackgroundTracingManagerImpl::GenerateMetadataProto(
    perfetto::protos::pbzero::ChromeMetadataPacket* metadata,
    bool privacy_filtering_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (legacy_active_scenario_) {
    legacy_active_scenario_->GenerateMetadataProto(metadata);
  } else if (active_scenario_) {
    active_scenario_->GenerateMetadataProto(metadata);
  }
}

void BackgroundTracingManagerImpl::AbortScenarioForTesting() {
  if (legacy_active_scenario_) {
    legacy_active_scenario_->AbortScenario();
  } else if (active_scenario_) {
    active_scenario_->Abort();
  }
}

void BackgroundTracingManagerImpl::OnScenarioAborted() {
  DCHECK(legacy_active_scenario_);

  for (auto* observer : background_tracing_observers_) {
    observer->OnScenarioIdle(
        legacy_active_scenario_->GetConfig()->scenario_name());
  }

  legacy_active_scenario_.reset();
}

void BackgroundTracingManagerImpl::CleanDatabase() {
  DCHECK(trace_database_);

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](TraceReportDatabase* trace_database) {
            trace_database->DeleteTracesOlderThan(kTraceTimeToLive);
            return trace_database->GetScenarioCounts();
          },
          base::Unretained(trace_database_.get())),
      base::BindOnce(&BackgroundTracingManagerImpl::OnTraceDatabaseUpdated,
                     weak_factory_.GetWeakPtr()));
}

void BackgroundTracingManagerImpl::DeleteTracesInDateRange(base::Time start,
                                                           base::Time end) {
  // The trace report database needs to exist for clean up. Avoid creating or
  // initializing the trace report database to perform a database clean up.
  absl::optional<base::FilePath> database_dir;
  if (!trace_database_) {
    database_dir = GetContentClient()->browser()->GetLocalTracesDirectory();
    if (database_dir.has_value()) {
      return;
    }
    trace_database_ = {new TraceReportDatabase,
                       base::OnTaskRunnerDeleter(database_task_runner_)};
  }
  auto on_database_updated =
      base::BindOnce(&BackgroundTracingManagerImpl::OnTraceDatabaseUpdated,
                     weak_factory_.GetWeakPtr());
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](TraceReportDatabase* trace_database,
             absl::optional<base::FilePath> database_dir, base::Time start,
             base::Time end,
             base::OnceCallback<void(ScenarioCountMap)> on_database_updated) {
            if (database_dir.has_value() &&
                !trace_database->OpenDatabaseIfExists(database_dir.value())) {
              return;
            }
            if (!trace_database->is_initialized()) {
              return;
            }
            if (trace_database->DeleteTracesInDateRange(start, end)) {
              GetUIThreadTaskRunner({})->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(on_database_updated),
                                 trace_database->GetScenarioCounts()));
            } else {
              RecordMetric(Metrics::DATABASE_CLEANUP_FAILED);
            }
          },
          base::Unretained(trace_database_.get()), database_dir, start, end,
          std::move(on_database_updated)));
}

// static
void BackgroundTracingManagerImpl::AddPendingAgent(
    int child_process_id,
    mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentProvider>
        pending_provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Delay agent initialization until we have an interested AgentObserver.
  // We set disconnect handler for cleanup when the tracing target is closed.
  mojo::Remote<tracing::mojom::BackgroundTracingAgentProvider> provider(
      std::move(pending_provider));

  provider.set_disconnect_handler(base::BindOnce(
      &BackgroundTracingManagerImpl::ClearPendingAgent, child_process_id));

  GetInstance().pending_agents_[child_process_id] = std::move(provider);
  GetInstance().MaybeConstructPendingAgents();
}

// static
void BackgroundTracingManagerImpl::ClearPendingAgent(int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetInstance().pending_agents_.erase(child_process_id);
}

void BackgroundTracingManagerImpl::MaybeConstructPendingAgents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (agent_observers_.empty())
    return;

  for (auto& pending_agent : pending_agents_) {
    pending_agent.second.set_disconnect_handler(base::OnceClosure());
    BackgroundTracingAgentClientImpl::Create(pending_agent.first,
                                             std::move(pending_agent.second));
  }
  pending_agents_.clear();
}

size_t BackgroundTracingManagerImpl::GetTraceUploadLimitKb() const {
#if BUILDFLAG(IS_ANDROID)
  auto type = net::NetworkChangeNotifier::GetConnectionType();
  if (net::NetworkChangeNotifier::IsConnectionCellular(type)) {
    return upload_limit_network_kb_;
  }
#endif
  return upload_limit_kb_;
}

}  // namespace content
