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

const char kBackgroundTracingConfig[] = "config";

// |g_background_tracing_manager| is intentionally leaked on shutdown.
BackgroundTracingManager* g_background_tracing_manager = nullptr;
BackgroundTracingManagerImpl* g_background_tracing_manager_impl = nullptr;

void OpenDatabaseOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    absl::optional<base::FilePath> database_dir,
    base::OnceCallback<void(absl::optional<BaseTraceReport>, bool)>
        on_database_created) {
  DCHECK(base::FeatureList::IsEnabled(kBackgroundTracingDatabase));
  if (database->is_initialized()) {
    return;
  }
  bool success;
  if (!database_dir) {
    success = database->OpenDatabaseInMemoryForTesting();  // IN-TEST
  } else {
    success = database->OpenDatabase(*database_dir);
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(on_database_created),
                     database->GetNextReportPendingUpload(), success));
}

absl::optional<NewTraceReport> AddTraceOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    std::string&& serialized_trace,
    const std::string& scenario_name,
    const std::string& rule_name,
    SkipUploadReason skip_reason) {
  std::string compressed_trace;
  bool success = compression::GzipCompress(serialized_trace, &compressed_trace);
  if (success) {
    UMA_HISTOGRAM_COUNTS_100000("Tracing.Background.CompressedTraceSizeInKB",
                                compressed_trace.size() / 1024);

    NewTraceReport trace_report;
    trace_report.uuid = base::Uuid::GenerateRandomV4();
    trace_report.creation_time = base::Time::Now();
    trace_report.scenario_name = scenario_name;
    trace_report.upload_rule_name = rule_name;
    trace_report.total_size = serialized_trace.size();
    trace_report.proto = std::move(compressed_trace);
    trace_report.skip_reason = skip_reason;

    // The database may be created even if BackgroundTracingDatabase is
    // disabled due to DeleteTracesInDateRange(). The trace is saved
    // only if BackgroundTracingDatabase is enabled and the database was
    // open successfully.
    if (database && database->is_initialized() &&
        base::FeatureList::IsEnabled(kBackgroundTracingDatabase)) {
      success = database->AddTrace(std::move(trace_report));
    } else {
      // Return the report as-is to hold the trace content in memory until it
      // can be uploaded.
      if (skip_reason == SkipUploadReason::kNoSkip) {
        return std::move(trace_report);
      }
    }
  }
  if (database && database->is_initialized() &&
      base::FeatureList::IsEnabled(kBackgroundTracingDatabase)) {
    BackgroundTracingManagerImpl::RecordMetric(
        success ? BackgroundTracingManagerImpl::Metrics::SAVE_TRACE_SUCCEEDED
                : BackgroundTracingManagerImpl::Metrics::SAVE_TRACE_FAILED);
    return database->GetNextReportPendingUpload();
  }
  return absl::nullopt;
}

void GetProtoValueOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    base::Uuid uuid,
    base::OnceCallback<void(std::string)> receive_callback,
    base::OnceCallback<void(absl::optional<BaseTraceReport>, bool)>
        on_finalize_complete) {
  DCHECK(base::FeatureList::IsEnabled(kBackgroundTracingDatabase));
  auto trace_content = database->GetProtoValue(uuid);
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
      .Run(trace_content ? *std::move(trace_content) : std::string());
}

}  // namespace

BASE_FEATURE(kBackgroundTracingDatabase,
             "BackgroundTracingDatabase",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
    const base::Uuid& trace_uuid,
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
    FinishedProcessingCallback callback) {
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
    const base::Uuid& trace_uuid,
    FinishedProcessingCallback callback) {
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

void BackgroundTracingManagerImpl::DownloadTrace(const base::Uuid& trace_uuid,
                                                 GetProtoCallback callback) {
  if (!trace_database_) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TraceReportDatabase::GetProtoValue,
                     base::Unretained(trace_database_.get()), trace_uuid),
      std::move(callback));
}

void BackgroundTracingManagerImpl::OnTraceDatabaseCreated(
    absl::optional<BaseTraceReport> trace_to_upload,
    bool creation_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

void BackgroundTracingManagerImpl::OnTraceSaved(
    absl::optional<NewTraceReport> trace_to_upload) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  trace_report_to_upload_ = std::move(trace_to_upload);
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

bool BackgroundTracingManagerImpl::InitializeScenarios(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config,
    ReceiveCallback receive_callback,
    DataFiltering data_filtering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!RequestActivateScenario()) {
    return false;
  }

  receive_callback_ = std::move(receive_callback);

  requires_anonymized_data_ = (data_filtering == ANONYMIZE_DATA);
  InitializeTraceReportDatabase();

  for (const auto& scenario_config : config.scenarios()) {
    auto scenario = TracingScenario::Create(
        scenario_config, requires_anonymized_data_, this, delegate_.get());
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
  // Pass a null ReceiveCallback to use the default upload behaviour.
  return SetActiveScenarioWithReceiveCallback(
      std::move(config), ReceiveCallback(), data_filtering);
}

bool BackgroundTracingManagerImpl::SetActiveScenarioWithReceiveCallback(
    std::unique_ptr<BackgroundTracingConfig> config,
    ReceiveCallback receive_callback,
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

  requires_anonymized_data_ = (data_filtering == ANONYMIZE_DATA);
  config_impl->set_requires_anonymized_data(requires_anonymized_data_);

  // TODO(oysteine): Retry when time_until_allowed has elapsed.
  if (delegate_ && !delegate_->IsAllowedToBeginBackgroundScenario(
                       config_impl->scenario_name(), requires_anonymized_data_,
                       config_impl->has_crash_scenario())) {
    return false;
  }

  receive_callback_ = std::move(receive_callback);
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
  if (!base::FeatureList::IsEnabled(kBackgroundTracingDatabase)) {
    return;
  }
  absl::optional<base::FilePath> database_dir;
  if (!trace_database_) {
    trace_database_ = {new TraceReportDatabase,
                       base::OnTaskRunnerDeleter(database_task_runner_)};
    if (!open_in_memory) {
      database_dir = GetContentClient()->browser()->GetLocalTracesDirectory();
      if (!database_dir.has_value()) {
        OnTraceDatabaseCreated(absl::nullopt, false);
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

void BackgroundTracingManagerImpl::OnScenarioActive(
    TracingScenario* active_scenario) {
  DCHECK_EQ(active_scenario_, nullptr);
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
}

void BackgroundTracingManagerImpl::OnScenarioIdle(
    TracingScenario* idle_scenario) {
  DCHECK_EQ(active_scenario_, idle_scenario);
  active_scenario_ = nullptr;
  for (auto* observer : background_tracing_observers_) {
    observer->OnScenarioIdle(idle_scenario->scenario_name());
  }
  for (auto& scenario : scenarios_) {
    scenario->Enable();
  }
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
                      triggered_rule->rule_id());
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
    base::OnceCallback<void(std::string)> receive_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!trace_report_to_upload_) {
    std::move(receive_callback).Run("");
    return;
  }

  // Trace content was kept in memory.
  if (!trace_report_to_upload_->proto.empty()) {
    std::move(receive_callback).Run(std::move(trace_report_to_upload_->proto));
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
    const std::string& rule_name) {
  InitializeTraceReportDatabase(true);
  OnProtoDataComplete(std::move(serialized_trace), scenario_name, rule_name);
}

void BackgroundTracingManagerImpl::OnProtoDataComplete(
    std::string&& serialized_trace,
    const std::string& scenario_name,
    const std::string& rule_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto* observer : background_tracing_observers_) {
    observer->OnTraceReceived(serialized_trace);
  }
  if (!receive_callback_) {
    SkipUploadReason skip_reason = SkipUploadReason::kNoSkip;
    if (serialized_trace.size() > upload_limit_kb_ * 1024) {
      skip_reason = SkipUploadReason::kSizeLimitExceeded;
    }
    if (skip_reason != SkipUploadReason::kNoSkip &&
        !(delegate_ && delegate_->ShouldSaveUnuploadedTrace())) {
      return;
    }
    BackgroundTracingManagerImpl::RecordMetric(Metrics::FINALIZATION_STARTED);
    UMA_HISTOGRAM_COUNTS_100000("Tracing.Background.FinalizingTraceSizeInKB2",
                                serialized_trace.size() / 1024);

    database_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(AddTraceOnDatabaseTaskRunner,
                       base::Unretained(trace_database_.get()),
                       std::move(serialized_trace), scenario_name, rule_name,
                       skip_reason),
        base::BindOnce(&BackgroundTracingManagerImpl::OnTraceSaved,
                       weak_factory_.GetWeakPtr()));
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
  }
  // TODO(crbug.com/1418116): Implement GenerateMetadataProto for
  // TracingScenario.
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

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&TraceReportDatabase::DeleteTracesOlderThan),
          base::Unretained(trace_database_.get()), kTraceTimeToLive));
}

void BackgroundTracingManagerImpl::DeleteTracesInDateRange(base::Time start,
                                                           base::Time end) {
  // The trace report database needs to exist for clean up. Avoid creating or
  // initializing the trace report database to perform a database clean up.
  absl::optional<base::FilePath> database_dir;
  if (!trace_database_) {
    database_dir = GetContentClient()->browser()->GetLocalTracesDirectory();
    if (!database_dir.has_value()) {
      return;
    }
    trace_database_ = {new TraceReportDatabase,
                       base::OnTaskRunnerDeleter(database_task_runner_)};
  }
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](TraceReportDatabase* trace_database,
             absl::optional<base::FilePath> database_dir, base::Time start,
             base::Time end) {
            if (database_dir.has_value() &&
                !trace_database->OpenDatabaseIfExists(database_dir.value())) {
              return;
            }
            if (!trace_database->is_initialized()) {
              return;
            }
            if (!trace_database->DeleteTracesInDateRange(start, end)) {
              RecordMetric(Metrics::DATABASE_CLEANUP_FAILED);
            }
          },
          base::Unretained(trace_database_.get()), database_dir, start, end));
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
