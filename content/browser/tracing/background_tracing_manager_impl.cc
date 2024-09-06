// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_manager_impl.h"

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/variations/hashing.h"
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
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/zlib/google/compression_utils.h"

namespace content {

namespace {
// The time to live of a trace report is currently 14 days.
const base::TimeDelta kTraceReportTimeToLive = base::Days(14);
// We limit the overall number of traces.
const size_t kMaxTraceContent = 200;
// We limit uploads of 1 trace per scenario over a period of 7 days. Since
// traces live in the database for longer than 7 days, their TTL doesn't affect
// this unless the database is manually cleared.
const base::TimeDelta kMinTimeUntilNextUpload = base::Days(7);
// We limit the overall number of traces per scenario saved to the database at
// 100 per day.
const size_t kMaxTracesPerScenario = 100;
const base::TimeDelta kMaxTracesPerScenarioDuration = base::Days(1);

const char kBackgroundTracingConfig[] = "config";

// |g_background_tracing_manager| is intentionally leaked on shutdown.
BackgroundTracingManager* g_background_tracing_manager = nullptr;
BackgroundTracingManagerImpl* g_background_tracing_manager_impl = nullptr;

void OpenDatabaseOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    std::optional<base::FilePath> database_dir,
    base::OnceCallback<void(BackgroundTracingManagerImpl::ScenarioCountMap,
                            std::optional<BaseTraceReport>,
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
  std::optional<NewTraceReport> report_to_upload;
  if (base::FeatureList::IsEnabled(kBackgroundTracingDatabase)) {
    report_to_upload = database->GetNextReportPendingUpload();
  } else {
    // Traces pending upload from previous sessions have timed out.
    database->AllPendingUploadSkipped(SkipUploadReason::kUploadTimedOut);
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(on_database_created),
                     database->GetScenarioCountsSince(
                         base::Time::Now() - kMaxTracesPerScenarioDuration),
                     std::move(report_to_upload), success));
}

void AddTraceOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    std::string&& serialized_trace,
    std::string&& serialized_system_profile,
    BaseTraceReport base_report,
    bool should_save_trace,
    bool is_crash_scenario,
    base::OnceCallback<void(std::optional<NewTraceReport>, bool)>
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
  std::optional<NewTraceReport> report_to_upload;
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
    base::OnceCallback<void(std::optional<std::string>,
                            std::optional<std::string>)> receive_callback,
    base::OnceCallback<void(std::optional<BaseTraceReport>, bool)>
        on_finalize_complete) {
  DCHECK(base::FeatureList::IsEnabled(kBackgroundTracingDatabase));
  auto trace_content = database->GetTraceContent(uuid);
  auto serialized_system_profile = database->GetSystemProfile(uuid);
  std::optional<ClientTraceReport> next_report;
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

class PreferenceManagerImpl
    : public BackgroundTracingManagerImpl::PreferenceManager {
 public:
  bool GetBackgroundStartupTracingEnabled() const override {
    return tracing::TraceStartupConfig::GetInstance().IsEnabled() &&
           tracing::TraceStartupConfig::GetInstance().GetSessionOwner() ==
               tracing::TraceStartupConfig::SessionOwner::kBackgroundTracing;
  }
};

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
    : delegate_(GetContentClient()->browser()->CreateTracingDelegate()),
      database_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      trace_database_(nullptr,
                      base::OnTaskRunnerDeleter(database_task_runner_)) {
  BackgroundTracingManager::SetInstance(this);
  NamedTriggerManager::SetInstance(this);
  g_background_tracing_manager_impl = this;
  preferences_ = std::make_unique<PreferenceManagerImpl>();
}

BackgroundTracingManagerImpl::~BackgroundTracingManagerImpl() {
  DCHECK_EQ(this, g_background_tracing_manager_impl);
  if (active_scenario_) {
    active_scenario_->Abort();
  } else {
    for (auto& scenario : enabled_scenarios_) {
      scenario->Disable();
    }
  }
  if (legacy_active_scenario_) {
    legacy_active_scenario_->AbortScenario();
  }
  for (auto& rule : trigger_rules_) {
    rule->Uninstall();
  }
  BackgroundTracingManager::SetInstance(nullptr);
  NamedTriggerManager::SetInstance(nullptr);
  g_background_tracing_manager_impl = nullptr;
}

void BackgroundTracingManagerImpl::OpenDatabaseIfExists() {
  if (trace_database_) {
    return;
  }
  std::optional<base::FilePath> database_dir =
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
    std::move(callback).Run(std::nullopt);
    return;
  }

  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TraceReportDatabase::GetTraceContent,
                     base::Unretained(trace_database_.get()), trace_uuid),
      base::BindOnce(
          [](GetProtoCallback callback,
             const std::optional<std::string>& result) {
            if (result) {
              std::move(callback).Run(base::span<const char>(*result));
            } else {
              std::move(callback).Run(std::nullopt);
            }
          },
          std::move(callback)));
}

void BackgroundTracingManagerImpl::OnTraceDatabaseCreated(
    ScenarioCountMap scenario_saved_counts,
    std::optional<BaseTraceReport> trace_to_upload,
    bool creation_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scenario_saved_counts_ = std::move(scenario_saved_counts);
  trace_report_to_upload_ = std::move(trace_to_upload);
  if (!creation_result) {
    RecordMetric(Metrics::DATABASE_INITIALIZATION_FAILED);
    return;
  }
  CleanDatabase();
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
    std::optional<NewTraceReport> trace_to_upload,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordMetric(success ? Metrics::SAVE_TRACE_SUCCEEDED
                       : Metrics::SAVE_TRACE_FAILED);
  trace_report_to_upload_ = std::move(trace_to_upload);
  if (success) {
    ++scenario_saved_counts_[scenario_name];
  }
  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
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
  DCHECK(field_scenarios_.empty());

  if (!enabled_scenarios_.empty()) {
    return false;
  }
  // Bail on scenario activation if trigger rules are already setup to be
  // forwarded to system tracing.
  if (!trigger_rules_.empty()) {
    return false;
  }
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

bool BackgroundTracingManagerImpl::InitializePerfettoTriggerRules(
    const perfetto::protos::gen::TracingTriggerRulesConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Trigger rules can't be initialized twice.
  DCHECK(trigger_rules_.empty());
  DCHECK_EQ(legacy_active_scenario_, nullptr);

  // Bail on setting up trigger rules if scenarios are already enabled.
  if (!enabled_scenarios_.empty()) {
    return false;
  }

  if (!BackgroundTracingRule::Append(config.rules(), trigger_rules_)) {
    return false;
  }
  for (auto& rule : trigger_rules_) {
    rule->Install(base::BindRepeating([](const BackgroundTracingRule* rule) {
      base::UmaHistogramSparse("Tracing.Background.Perfetto.Trigger",
                               variations::HashName(rule->rule_id()));
      perfetto::Tracing::ActivateTriggers({rule->rule_id()},
                                          /*ttl_ms=*/0);
      return true;
    }));
  }
  return true;
}

bool BackgroundTracingManagerImpl::InitializeFieldScenarios(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config,
    DataFiltering data_filtering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(legacy_active_scenario_, nullptr);
  if (!RequestActivateScenario()) {
    return false;
  }

  bool requires_anonymized_data = (data_filtering != NO_DATA_FILTERING);
  bool enable_package_name_filter =
      (data_filtering == ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME);
  InitializeTraceReportDatabase();

  // Guaranteed by RequestActivateScenario() above.
  DCHECK(enabled_scenarios_.empty());

  if (preferences_->GetBackgroundStartupTracingEnabled()) {
    perfetto::protos::gen::ScenarioConfig scenario_config;
    scenario_config.set_scenario_name("Startup");
    *scenario_config.mutable_trace_config() =
        tracing::TraceStartupConfig::GetDefaultBackgroundStartupConfig();
    scenario_config.add_start_rules()->set_manual_trigger_name(
        base::trace_event::kStartupTracingTriggerName);
    scenario_config.add_upload_rules()->set_delay_ms(30000);

    // Startup tracing was already requested earlier for this scenario.
    auto startup_scenario = TracingScenario::Create(
        scenario_config, requires_anonymized_data, enable_package_name_filter,
        /*request_startup_tracing=*/false, this);
    field_scenarios_.push_back(std::move(startup_scenario));
    enabled_scenarios_.push_back(field_scenarios_.back().get());
    enabled_scenarios_.back()->Enable();
  }

  for (const auto& scenario_config : config.scenarios()) {
    auto scenario =
        TracingScenario::Create(scenario_config, requires_anonymized_data,
                                enable_package_name_filter, true, this);
    if (!scenario) {
      return false;
    }
    field_scenarios_.push_back(std::move(scenario));
    enabled_scenarios_.push_back(field_scenarios_.back().get());
    enabled_scenarios_.back()->Enable();
  }
  RecordMetric(Metrics::SCENARIO_ACTIVATED_SUCCESSFULLY);
  return true;
}

std::vector<std::string> BackgroundTracingManagerImpl::AddPresetScenarios(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config,
    DataFiltering data_filtering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool enable_privacy_filter = (data_filtering != NO_DATA_FILTERING);
  bool enable_package_name_filter =
      (data_filtering == ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME);

  std::vector<std::string> added_scenarios;
  for (const auto& scenario_config : config.scenarios()) {
    auto scenario =
        TracingScenario::Create(scenario_config, enable_privacy_filter,
                                enable_package_name_filter, true, this);
    if (!scenario) {
      continue;
    }
    added_scenarios.push_back(scenario->config_hash());
    preset_scenarios_.emplace(scenario->config_hash(), std::move(scenario));
  }
  return added_scenarios;
}

std::vector<trace_report::mojom::ScenarioPtr>
BackgroundTracingManagerImpl::GetAllFieldScenarios() const {
  std::vector<trace_report::mojom::ScenarioPtr> result;
  for (const auto& scenario : field_scenarios_) {
    auto new_scenario = trace_report::mojom::Scenario::New();
    new_scenario->hash = scenario->config_hash();
    new_scenario->scenario_name = scenario->scenario_name();
    result.push_back(std::move(new_scenario));
  }
  return result;
}

std::vector<trace_report::mojom::ScenarioPtr>
BackgroundTracingManagerImpl::GetAllPresetScenarios() const {
  std::vector<trace_report::mojom::ScenarioPtr> result;
  for (const auto& scenario : preset_scenarios_) {
    auto new_scenario = trace_report::mojom::Scenario::New();
    new_scenario->hash = scenario.first;
    new_scenario->scenario_name = scenario.second->scenario_name();
    result.push_back(std::move(new_scenario));
  }
  return result;
}

bool BackgroundTracingManagerImpl::SetEnabledScenarios(
    std::vector<std::string> enabled_scenarios) {
  if (active_scenario_) {
    enabled_scenarios_.clear();
    active_scenario_->Abort();
  } else {
    for (auto& scenario : enabled_scenarios_) {
      scenario->Disable();
    }
    enabled_scenarios_.clear();
  }
  for (auto& rule : trigger_rules_) {
    rule->Uninstall();
  }
  trigger_rules_.clear();
  InitializeTraceReportDatabase();
  for (const std::string& hash : enabled_scenarios) {
    auto it = preset_scenarios_.find(hash);
    if (it == preset_scenarios_.end()) {
      return false;
    }
    enabled_scenarios_.push_back(it->second.get());
    if (!active_scenario_) {
      it->second->Enable();
    }
  }
  return true;
}

std::vector<std::string> BackgroundTracingManagerImpl::GetEnabledScenarios()
    const {
  std::vector<std::string> scenario_hashes;
  for (auto scenario : enabled_scenarios_) {
    scenario_hashes.push_back(scenario->config_hash());
  }
  return scenario_hashes;
}

bool BackgroundTracingManagerImpl::SetActiveScenario(
    std::unique_ptr<BackgroundTracingConfig> config,
    DataFiltering data_filtering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!config) {
    return false;
  }
  std::unique_ptr<BackgroundTracingConfigImpl> config_impl(
      static_cast<BackgroundTracingConfigImpl*>(config.release()));

  if (!RequestActivateScenario()) {
    return false;
  }

  if (config_impl->upload_limit_kb()) {
    upload_limit_kb_ = *config_impl->upload_limit_kb();
  }
  if (config_impl->upload_limit_network_kb()) {
    upload_limit_network_kb_ = *config_impl->upload_limit_network_kb();
  }

  bool requires_anonymized_data = (data_filtering != NO_DATA_FILTERING);
  config_impl->set_requires_anonymized_data(requires_anonymized_data);

  bool enable_package_name_filter =
      (data_filtering == ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME);
  config_impl->SetPackageNameFilteringEnabled(enable_package_name_filter);

  // TODO(oysteine): Retry when time_until_allowed has elapsed.
  if (delegate_ &&
      !delegate_->OnBackgroundTracingActive(requires_anonymized_data)) {
    return false;
  }

  legacy_active_scenario_ = std::make_unique<BackgroundTracingActiveScenario>(
      std::move(config_impl), delegate_.get(),
      base::BindOnce(&BackgroundTracingManagerImpl::OnScenarioAborted,
                     base::Unretained(this)));
  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
    observer->OnScenarioActive(
        legacy_active_scenario_->GetConfig()->scenario_name());
  }

  InitializeTraceReportDatabase();

  legacy_active_scenario_->StartTracingIfConfigNeedsIt();
  RecordMetric(Metrics::SCENARIO_ACTIVATED_SUCCESSFULLY);

  return true;
}

void BackgroundTracingManagerImpl::InitializeTraceReportDatabase(
    bool open_in_memory) {
  std::optional<base::FilePath> database_dir;
  if (!trace_database_) {
    trace_database_ = {new TraceReportDatabase,
                       base::OnTaskRunnerDeleter(database_task_runner_)};
    if (!open_in_memory) {
      database_dir = GetContentClient()->browser()->GetLocalTracesDirectory();
      if (!database_dir.has_value()) {
        OnTraceDatabaseCreated({}, std::nullopt, false);
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
  if (delegate_ && !delegate_->OnBackgroundTracingActive(
                       active_scenario->privacy_filter_enabled())) {
    return false;
  }
  active_scenario_ = active_scenario;
  base::UmaHistogramSparse(
      "Tracing.Background.Scenario.Active",
      variations::HashName(active_scenario->scenario_name()));
  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
    observer->OnScenarioActive(active_scenario_->scenario_name());
  }
  for (auto& scenario : enabled_scenarios_) {
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
  base::UmaHistogramSparse(
      "Tracing.Background.Scenario.Idle",
      variations::HashName(idle_scenario->scenario_name()));
  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
    observer->OnScenarioIdle(idle_scenario->scenario_name());
  }
  bool is_allowed_finalization =
      !delegate_ || delegate_->OnBackgroundTracingIdle(
                        idle_scenario->privacy_filter_enabled());
  for (auto& scenario : enabled_scenarios_) {
    scenario->Enable();
  }
  return is_allowed_finalization;
}

void BackgroundTracingManagerImpl::OnScenarioRecording(
    TracingScenario* scenario) {
  DCHECK_EQ(active_scenario_, scenario);
  base::UmaHistogramSparse("Tracing.Background.Scenario.Recording",
                           variations::HashName(scenario->scenario_name()));
  OnStartTracingDone();
}

void BackgroundTracingManagerImpl::SaveTrace(
    TracingScenario* scenario,
    base::Token trace_uuid,
    const BackgroundTracingRule* triggered_rule,
    std::string&& trace_data) {
  std::string rule_name = triggered_rule->rule_id();
  if (triggered_rule->triggered_value()) {
    rule_name.append(
        base::StringPrintf(" value: %d", *triggered_rule->triggered_value()));
  }
  OnProtoDataComplete(std::move(trace_data), scenario->scenario_name(),
                      rule_name, scenario->privacy_filter_enabled(),
                      /*is_crash_scenario=*/false, trace_uuid);
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
    base::OnceCallback<void(std::optional<std::string>,
                            std::optional<std::string>)> receive_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!trace_report_to_upload_) {
    std::move(receive_callback).Run(std::nullopt, std::nullopt);
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
    OnFinalizeComplete(std::nullopt, true);
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
    std::optional<BaseTraceReport> trace_to_upload,
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

  for (AgentObserver* observer : agent_observers_) {
    observer->OnAgentAdded(agent);
  }
}

void BackgroundTracingManagerImpl::RemoveAgent(
    tracing::mojom::BackgroundTracingAgent* agent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (AgentObserver* observer : agent_observers_) {
    observer->OnAgentRemoved(agent);
  }

  agents_.erase(agent);
}

void BackgroundTracingManagerImpl::AddAgentObserver(AgentObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agent_observers_.insert(observer);

  MaybeConstructPendingAgents();

  for (tracing::mojom::BackgroundTracingAgent* agent : agents_) {
    observer->OnAgentAdded(agent);
  }
}

void BackgroundTracingManagerImpl::RemoveAgentObserver(
    AgentObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agent_observers_.erase(observer);

  for (tracing::mojom::BackgroundTracingAgent* agent : agents_) {
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
                      /*privacy_filter_enabled*/ true,
                      /*is_crash_scenario=*/false, uuid);
}

void BackgroundTracingManagerImpl::SetPreferenceManagerForTesting(
    std::unique_ptr<PreferenceManager> preferences) {
  preferences_ = std::move(preferences);
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
    bool privacy_filter_enabled,
    bool is_crash_scenario,
    const base::Token& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
    observer->OnTraceReceived(serialized_trace);
  }
  if (!receive_callback_) {
    DCHECK(trace_database_);

    base::UmaHistogramSparse("Tracing.Background.Scenario.SaveTrace",
                             variations::HashName(scenario_name));

    SkipUploadReason skip_reason = SkipUploadReason::kNoSkip;
    if (!privacy_filter_enabled) {
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
        uuid.ToString() + ".perfetto.gz", std::move(serialized_trace),
        base::BindOnce(&BackgroundTracingManagerImpl::OnFinalizeComplete,
                       weak_factory_.GetWeakPtr(), std::nullopt));
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

void BackgroundTracingManagerImpl::AddNamedTriggerObserver(
    const std::string& trigger_name,
    BackgroundTracingRule* observer) {
  named_trigger_observers_[trigger_name].AddObserver(observer);
}

void BackgroundTracingManagerImpl::RemoveNamedTriggerObserver(
    const std::string& trigger_name,
    BackgroundTracingRule* observer) {
  named_trigger_observers_[trigger_name].RemoveObserver(observer);
}

bool BackgroundTracingManagerImpl::DoEmitNamedTrigger(
    const std::string& trigger_name,
    std::optional<int32_t> value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = named_trigger_observers_.find(trigger_name);
  if (it == named_trigger_observers_.end()) {
    return false;
  }
  for (BackgroundTracingRule& obs : it->second) {
    if (obs.OnRuleTriggered(value)) {
      return true;
    }
  }
  return false;
}

void BackgroundTracingManagerImpl::InvalidateTriggersCallbackForTesting() {
  named_trigger_observers_.clear();
}

void BackgroundTracingManagerImpl::OnStartTracingDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
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

  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
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
            // Trace payload is cleared on a more frequent basis.
            trace_database->DeleteOldTraceContent(kMaxTraceContent);
            // The reports entries are kept (without the payload) for longer to
            // track upload quotas.
            trace_database->DeleteTraceReportsOlderThan(kTraceReportTimeToLive);
            return trace_database->GetScenarioCountsSince(
                base::Time::Now() - kMaxTracesPerScenarioDuration);
          },
          base::Unretained(trace_database_.get())),
      base::BindOnce(&BackgroundTracingManagerImpl::OnTraceDatabaseUpdated,
                     weak_factory_.GetWeakPtr()));
}

void BackgroundTracingManagerImpl::DeleteTracesInDateRange(base::Time start,
                                                           base::Time end) {
  // The trace report database needs to exist for clean up. Avoid creating or
  // initializing the trace report database to perform a database clean up.
  std::optional<base::FilePath> database_dir;
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
             std::optional<base::FilePath> database_dir, base::Time start,
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
                  base::BindOnce(
                      std::move(on_database_updated),
                      trace_database->GetScenarioCountsSince(
                          base::Time::Now() - kMaxTracesPerScenarioDuration)));
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
