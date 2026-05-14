// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/background_tracing/background_tracing_manager.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/trace_time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/variations/hashing.h"
#include "net/base/network_change_notifier.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_rule.h"
#include "services/tracing/public/cpp/background_tracing/trace_report_database.h"
#include "services/tracing/public/cpp/background_tracing/triggers_data_source.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"
#include "third_party/perfetto/protos/perfetto/common/data_source_descriptor.gen.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tracing {

namespace {
// The time to live of a trace report is currently 14 days.
constexpr base::TimeDelta kTraceReportTimeToLive = base::Days(14);
// The time to live of uploaded trace content is 2 days.
constexpr base::TimeDelta kUploadedTraceContentTimeToLive = base::Days(2);
// We limit the overall number of traces.
constexpr size_t kMaxTraceContent = 200;
// We limit uploads of 1 trace per scenario over a period of 7 days. Since
// traces live in the database for longer than 7 days, their TTL doesn't affect
// this unless the database is manually cleared.
constexpr base::TimeDelta kMinTimeUntilNextUpload = base::Days(7);
// We limit the overall number of traces per scenario saved to the database at
// 100 per day.
constexpr size_t kMaxTracesPerScenario = 100;
constexpr base::TimeDelta kMaxTracesPerScenarioDuration = base::Days(1);

// |g_background_tracing_manager| is intentionally leaked on shutdown.
BackgroundTracingManager* g_background_tracing_manager = nullptr;

void OpenDatabaseOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    std::optional<base::FilePath> database_dir,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
    base::OnceCallback<void(BackgroundTracingManager::ScenarioCountMap,
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
  std::optional<NewTraceReport> report_to_upload =
      database->GetNextReportPendingUpload();
  reply_task_runner->PostTask(
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
    bool force_upload,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
    base::OnceCallback<void(std::optional<BaseTraceReport>, bool)>
        on_trace_saved) {
  if (!database->is_initialized()) {
    return;
  }
  base::Time since = base::Time::Now() - kMinTimeUntilNextUpload;
  auto upload_count = database->UploadCountSince(
      base_report.scenario_name, base_report.upload_rule_name, since);
  if (base_report.skip_reason == SkipUploadReason::kNoSkip && !force_upload &&
      upload_count && *upload_count > 0) {
    base_report.skip_reason = SkipUploadReason::kScenarioQuotaExceeded;
    if (!should_save_trace) {
      return;
    }
  }

  std::string compressed_trace;
  bool success = compression::GzipCompress(serialized_trace, &compressed_trace);
  if (success) {
    NewTraceReport trace_report = base_report;
    trace_report.trace_content = std::move(compressed_trace);
    trace_report.system_profile = std::move(serialized_system_profile);
    success = database->AddTrace(trace_report);
  }
  auto report_to_upload = database->GetNextReportPendingUpload();
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_trace_saved),
                                std::move(report_to_upload), success));
}

void OnUploadCompleteOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    BaseTraceReport base_report,
    scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
    base::OnceCallback<void(std::optional<BaseTraceReport>, bool)>
        on_finalize_complete) {
  base::Token uuid = base_report.uuid;
  base::UmaHistogramSparse("Tracing.Background.Scenario.Upload",
                           variations::HashName(base_report.scenario_name));
  std::optional<ClientTraceReport> next_report;
  if (database->UploadComplete(uuid, base::Time::Now())) {
    next_report = database->GetNextReportPendingUpload();
  }
  reply_task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(on_finalize_complete),
                                             std::move(next_report), true));
}

void GetProtoValueOnDatabaseTaskRunner(
    TraceReportDatabase* database,
    BaseTraceReport base_report,
    base::OnceCallback<void(std::optional<std::string>,
                            std::optional<std::string>,
                            base::OnceClosure)> receive_callback,
    base::OnceClosure upload_complete) {
  base::Token uuid = base_report.uuid;
  auto compressed_trace_content = database->GetTraceContent(uuid);
  if (!compressed_trace_content) {
    std::move(receive_callback)
        .Run(std::nullopt, std::nullopt, base::NullCallback());
  } else {
    auto serialized_system_profile = database->GetSystemProfile(uuid);
    std::move(receive_callback)
        .Run(std::move(compressed_trace_content),
             std::move(serialized_system_profile), std::move(upload_complete));
  }
}

// Emits background tracing metadata as a data source.
class BackgroundMetadataDataSource
    : public perfetto::DataSource<BackgroundMetadataDataSource> {
 public:
  static constexpr bool kRequiresCallbacksUnderLock = false;

  static void Register() {
    perfetto::DataSourceDescriptor desc;
    desc.set_name("org.chromium.background_scenario_metadata");
    CHECK(perfetto::DataSource<BackgroundMetadataDataSource>::Register(desc));
  }

  static void EmitMetadata(TracingScenario* scenario) {
    Trace([&](TraceContext ctx) {
      auto packet = ctx.NewTracePacket();
      packet->set_timestamp(
          TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds());
      packet->set_timestamp_clock_id(base::tracing::kTraceClockId);
      auto* chrome_metadata = packet->set_chrome_metadata();
      scenario->GenerateMetadataProto(chrome_metadata);
      packet->Finalize();
      ctx.Flush();
    });
  }
};

}  // namespace

// static
BackgroundTracingManager& BackgroundTracingManager::GetInstance() {
  CHECK_NE(nullptr, g_background_tracing_manager);
  return *g_background_tracing_manager;
}

// static
void BackgroundTracingManager::RecordMetric(Metrics metric) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.ScenarioState", metric,
                            Metrics::NUMBER_OF_BACKGROUND_TRACING_METRICS);
}

BackgroundTracingManager::BackgroundTracingManager()
    : database_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      trace_database_(nullptr,
                      base::OnTaskRunnerDeleter(database_task_runner_)) {
  DCHECK_EQ(g_background_tracing_manager, nullptr);
  base::trace_event::NamedTriggerManager::SetInstance(this);
  g_background_tracing_manager = this;
  if (perfetto::Tracing::IsInitialized()) {
    AddMetadataGeneratorFunction();
  }
}

BackgroundTracingManager::~BackgroundTracingManager() {
  DCHECK_EQ(this, g_background_tracing_manager);
  DisableScenarios();
  g_background_tracing_manager = nullptr;
  base::trace_event::NamedTriggerManager::SetInstance(nullptr);
}

void BackgroundTracingManager::OpenDatabaseIfExists() {
  if (trace_database_) {
    return;
  }
  std::optional<base::FilePath> database_dir = GetLocalTracesDirectory();
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

void BackgroundTracingManager::GetAllTraceReports(GetReportsCallback callback) {
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

void BackgroundTracingManager::DeleteSingleTrace(
    const base::Token& trace_uuid,
    TraceUploadList::FinishedProcessingCallback callback) {
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

void BackgroundTracingManager::DeleteAllTraces(
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

void BackgroundTracingManager::UserUploadSingleTrace(
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

void BackgroundTracingManager::DownloadTrace(const base::Token& trace_uuid,
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

void BackgroundTracingManager::OnTraceDatabaseCreated(
    ScenarioCountMap scenario_saved_counts,
    std::optional<BaseTraceReport> trace_to_upload,
    bool creation_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scenario_saved_counts_ = std::move(scenario_saved_counts);
  trace_report_to_upload_ = std::move(trace_to_upload);
  if (!creation_result) {
    RecordMetric(Metrics::DATABASE_INITIALIZATION_FAILED);
    return;
  }
  CleanDatabase();
  clean_database_timer_.Start(
      FROM_HERE, base::Days(1),
      base::BindRepeating(&BackgroundTracingManager::CleanDatabase,
                          weak_factory_.GetWeakPtr()));
}

void BackgroundTracingManager::OnTraceDatabaseUpdated(
    ScenarioCountMap scenario_saved_counts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scenario_saved_counts_ = std::move(scenario_saved_counts);
}

void BackgroundTracingManager::OnTraceSaved(
    const std::string& scenario_name,
    std::optional<BaseTraceReport> trace_to_upload,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void BackgroundTracingManager::AddMetadataGeneratorFunction() {
  BackgroundMetadataDataSource::Register();
  TriggersDataSource::Register();
}

bool BackgroundTracingManager::GetBackgroundStartupTracingEnabled() const {
  return false;
}

bool BackgroundTracingManager::RequestActivateScenario() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Multi-scenarios sessions can't be initialized twice.
  DCHECK(field_scenarios_.empty());
  DCHECK(enabled_scenarios_.empty());
  RecordMetric(Metrics::SCENARIO_ACTIVATION_REQUESTED);

  // Bail on scenario activation if trigger rules are already setup to be
  // forwarded to system tracing.
  if (!trigger_rules_.empty()) {
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

void BackgroundTracingManager::DisableScenarios() {
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
}

void BackgroundTracingManager::SetReceiveCallback(
    ReceiveCallback receive_callback) {
  receive_callback_ = std::move(receive_callback);
}

bool BackgroundTracingManager::InitializePerfettoTriggerRules(
    const perfetto::protos::gen::TracingTriggerRulesConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Trigger rules can't be initialized twice.
  DCHECK(trigger_rules_.empty());

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
                               variations::HashName(rule->rule_name()));
      perfetto::Tracing::ActivateTriggers({rule->rule_name()},
                                          /*ttl_ms=*/0);
      return true;
    }));
  }
  return true;
}

bool BackgroundTracingManager::InitializeFieldScenarios(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config,
    DataFiltering data_filtering,
    bool force_uploads,
    size_t upload_limit_kb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!RequestActivateScenario()) {
    return false;
  }
  force_uploads_ = force_uploads;
  if (upload_limit_kb > 0) {
    upload_limit_kb_ = upload_limit_kb;
  }

  bool requires_anonymized_data = (data_filtering != NO_DATA_FILTERING);
  bool enable_package_name_filter =
      (data_filtering == ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME);
  InitializeTraceReportDatabase();

  if (GetBackgroundStartupTracingEnabled()) {
    perfetto::protos::gen::ScenarioConfig scenario_config;
    scenario_config.set_scenario_name("Startup");
    *scenario_config.mutable_trace_config() =
        TraceStartupConfig::GetDefaultBackgroundStartupConfig();
    scenario_config.add_start_rules()->set_manual_trigger_name(
        base::trace_event::kStartupTracingTriggerName);
    scenario_config.add_upload_rules()->set_delay_ms(30000);

    // Startup tracing was already requested earlier for this scenario.
    auto startup_scenario = TracingScenario::Create(
        scenario_config, requires_anonymized_data,
        /*is_local_scenario=*/false, enable_package_name_filter,
        /*request_startup_tracing=*/false, this);
    field_scenarios_.push_back(std::move(startup_scenario));
    enabled_scenarios_.push_back(field_scenarios_.back().get());
    enabled_scenarios_.back()->Enable();
  }

  bool result = true;
  for (const auto& scenario_config : config.scenarios()) {
    auto scenario = TracingScenario::Create(
        scenario_config, requires_anonymized_data,
        /*is_local_scenario=*/false, enable_package_name_filter, true, this);
    if (!scenario) {
      base::UmaHistogramSparse(
          "Tracing.Background.Scenario.Invalid",
          variations::HashName(scenario_config.scenario_name()));
      result = false;
      continue;
    }
    field_scenarios_.push_back(std::move(scenario));
    enabled_scenarios_.push_back(field_scenarios_.back().get());
    enabled_scenarios_.back()->Enable();
  }
  MaybeConstructPendingAgents();
  RecordMetric(Metrics::SCENARIO_ACTIVATED_SUCCESSFULLY);
  return result;
}

std::vector<std::string> BackgroundTracingManager::AddPresetScenarios(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config,
    DataFiltering data_filtering) {
  return AddPresetScenariosImpl(config, data_filtering, false);
}

std::vector<std::string> BackgroundTracingManager::OverwritePresetScenarios(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config,
    DataFiltering data_filtering) {
  return AddPresetScenariosImpl(config, data_filtering, true);
}

std::vector<std::string> BackgroundTracingManager::AddPresetScenariosImpl(
    const perfetto::protos::gen::ChromeFieldTracingConfig& config,
    DataFiltering data_filtering,
    bool overwrite_conflicts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool enable_privacy_filter = (data_filtering != NO_DATA_FILTERING);
  bool enable_package_name_filter =
      (data_filtering == ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME);

  std::vector<std::string> added_scenarios;
  std::set<raw_ptr<TracingScenario>> conflicting_scenarios_set;
  std::vector<std::unique_ptr<TracingScenario>> conflicting_scenarios;
  for (const auto& scenario_config : config.scenarios()) {
    auto scenario = TracingScenario::Create(
        scenario_config, enable_privacy_filter, /*is_local_scenario=*/true,
        enable_package_name_filter, true, this);
    if (!scenario) {
      base::UmaHistogramSparse(
          "Tracing.Background.Scenario.Invalid",
          variations::HashName(scenario_config.scenario_name()));
      continue;
    }

    if (auto it = preset_scenarios_.find(scenario_config.scenario_name());
        it != preset_scenarios_.end()) {
      if (!overwrite_conflicts) {
        continue;
      }
      if (active_scenario_ == it->second.get()) {
        active_scenario_->Abort();
        active_scenario_ = nullptr;
        conflicting_scenarios_set.insert(it->second.get());
        conflicting_scenarios.emplace_back(std::move(it->second));
      } else if (it->second->current_state() !=
                 TracingScenario::State::kDisabled) {
        it->second->Disable();
        conflicting_scenarios_set.insert(it->second.get());
        conflicting_scenarios.emplace_back(std::move(it->second));
      }
    }

    added_scenarios.push_back(scenario->scenario_name());
    preset_scenarios_[scenario->scenario_name()] = std::move(scenario);
  }
  if (!conflicting_scenarios.empty()) {
    std::erase_if(enabled_scenarios_, [&](raw_ptr<TracingScenario> scenario) {
      return conflicting_scenarios_set.contains(scenario);
    });
  }
  conflicting_scenarios_set.clear();

  return added_scenarios;
}

bool BackgroundTracingManager::SetEnabledScenarios(
    std::vector<std::string> enabled_scenarios) {
  DisableScenarios();
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
  MaybeConstructPendingAgents();
  return true;
}

std::vector<std::string> BackgroundTracingManager::GetEnabledScenarios() const {
  std::vector<std::string> scenario_hashes;
  for (auto scenario : enabled_scenarios_) {
    scenario_hashes.push_back(scenario->scenario_name());
  }
  return scenario_hashes;
}

void BackgroundTracingManager::InitializeTraceReportDatabase(
    bool open_in_memory) {
  std::optional<base::FilePath> database_dir;
  if (!trace_database_) {
    trace_database_ = {new TraceReportDatabase,
                       base::OnTaskRunnerDeleter(database_task_runner_)};
    if (!open_in_memory) {
      database_dir = GetLocalTracesDirectory();
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
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&BackgroundTracingManager::OnTraceDatabaseCreated,
                         weak_factory_.GetWeakPtr())));
}

bool BackgroundTracingManager::OnScenarioActive(
    TracingScenario* active_scenario) {
  DCHECK_EQ(active_scenario_, nullptr);
  if (GetScenarioSavedCount(active_scenario->scenario_name()) >=
      kMaxTracesPerScenario) {
    return false;
  }
  auto now = base::TimeTicks::Now();
  if (!IsRecordingAllowed(active_scenario->privacy_filter_enabled(), now)) {
    return false;
  }
  scenario_start_time_ = now;
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

bool BackgroundTracingManager::OnScenarioIdle(TracingScenario* idle_scenario) {
  DCHECK_EQ(active_scenario_, idle_scenario);
  active_scenario_ = nullptr;
  base::UmaHistogramSparse(
      "Tracing.Background.Scenario.Idle",
      variations::HashName(idle_scenario->scenario_name()));
  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
    observer->OnScenarioIdle(idle_scenario->scenario_name());
  }
  for (auto& scenario : enabled_scenarios_) {
    scenario->Enable();
  }
  return IsRecordingAllowed(idle_scenario->privacy_filter_enabled(),
                            scenario_start_time_);
}

void BackgroundTracingManager::OnScenarioError(TracingScenario* scenario,
                                               perfetto::TracingError error) {
  base::UmaHistogramSparse("Tracing.Background.Scenario.Error",
                           variations::HashName(scenario->scenario_name()));
  DLOG(ERROR) << "Background tracing error: " << error.message;
}

bool BackgroundTracingManager::OnScenarioCloned(
    TracingScenario* cloned_scenario) {
  DCHECK_EQ(active_scenario_, cloned_scenario);
  base::UmaHistogramSparse(
      "Tracing.Background.Scenario.Clone",
      variations::HashName(cloned_scenario->scenario_name()));
  return IsRecordingAllowed(cloned_scenario->privacy_filter_enabled(),
                            scenario_start_time_);
}

void BackgroundTracingManager::OnScenarioRecording(TracingScenario* scenario) {
  DCHECK_EQ(active_scenario_, scenario);
  base::UmaHistogramSparse("Tracing.Background.Scenario.Recording",
                           variations::HashName(scenario->scenario_name()));
  BackgroundMetadataDataSource::EmitMetadata(scenario);
  OnStartTracingDone();
}

void BackgroundTracingManager::SaveTrace(
    TracingScenario* scenario,
    base::Token trace_uuid,
    const BackgroundTracingRule* triggered_rule,
    std::string&& trace_data) {
  OnProtoDataComplete(
      std::move(trace_data), scenario->scenario_name(),
      triggered_rule->rule_name(), triggered_rule->triggered_value(),
      scenario->privacy_filter_enabled(), scenario->is_local_scenario(),
      /*force_upload=*/force_uploads_, trace_uuid);
}

bool BackgroundTracingManager::HasActiveScenario() {
  return active_scenario_ != nullptr;
}

bool BackgroundTracingManager::HasTraceToUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!trace_report_to_upload_) {
    return false;
  }
#if BUILDFLAG(IS_ANDROID)
  // Send the logs only when the trace size is within limits. If the connection
  // type changes and we have a bigger than expected trace, then the next time
  // service asks us when wifi is available, the trace will be sent.
  auto type = net::NetworkChangeNotifier::GetConnectionType();
  if (net::NetworkChangeNotifier::IsConnectionCellular(type) &&
      trace_report_to_upload_->total_size > upload_limit_network_kb_ * 1000) {
    RecordMetric(Metrics::LARGE_UPLOAD_WAITING_TO_RETRY);
    return false;
  }
#endif
  return true;
}

void BackgroundTracingManager::GetTraceToUpload(
    base::OnceCallback<void(std::optional<std::string>,
                            std::optional<std::string>,
                            base::OnceClosure)> receive_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!trace_report_to_upload_) {
    std::move(receive_callback)
        .Run(std::nullopt, std::nullopt, base::NullCallback());
    return;
  }

  DCHECK(trace_database_);
  BaseTraceReport trace_report = *std::move(trace_report_to_upload_);
  trace_report_to_upload_.reset();
  auto upload_complete_callback = base::BindPostTask(
      database_task_runner_,
      base::BindOnce(
          OnUploadCompleteOnDatabaseTaskRunner,
          base::Unretained(trace_database_.get()), trace_report,
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&BackgroundTracingManager::OnFinalizeComplete,
                         weak_factory_.GetWeakPtr())));
  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(GetProtoValueOnDatabaseTaskRunner,
                                base::Unretained(trace_database_.get()),
                                trace_report, std::move(receive_callback),
                                std::move(upload_complete_callback)));
}

void BackgroundTracingManager::OnFinalizeComplete(
    std::optional<BaseTraceReport> trace_to_upload,
    bool success) {
  trace_report_to_upload_ = std::move(trace_to_upload);
  if (success) {
    RecordMetric(Metrics::UPLOAD_SUCCEEDED);
  } else {
    RecordMetric(Metrics::UPLOAD_FAILED);
  }
}

void BackgroundTracingManager::AddEnabledStateObserverForTesting(
    EnabledStateTestObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_tracing_observers_.insert(observer);
}

void BackgroundTracingManager::RemoveEnabledStateObserverForTesting(
    EnabledStateTestObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_tracing_observers_.erase(observer);
}

bool BackgroundTracingManager::IsTracingForTesting() {
  return active_scenario_->current_state() ==
         TracingScenario::State::kRecording;
}

void BackgroundTracingManager::SaveTraceForTesting(
    std::string&& serialized_trace,
    const std::string& scenario_name,
    const std::string& rule_name,
    const base::Token& uuid) {
  InitializeTraceReportDatabase(true);
  OnProtoDataComplete(std::move(serialized_trace), scenario_name, rule_name,
                      /*rule_value=*/std::nullopt,
                      /*privacy_filter_enabled*/ true,
                      /*is_local_scenario=*/false,
                      /*force_upload=*/force_uploads_, uuid);
}

void BackgroundTracingManager::SetUploadLimitsForTesting(
    size_t upload_limit_kb,
    size_t upload_limit_network_kb) {
  upload_limit_kb_ = upload_limit_kb;
  upload_limit_network_kb_ = upload_limit_network_kb;
}

size_t BackgroundTracingManager::GetScenarioSavedCount(
    const std::string& scenario_name) {
  auto it = scenario_saved_counts_.find(scenario_name);
  if (it != scenario_saved_counts_.end()) {
    return it->second;
  }
  return 0;
}

void BackgroundTracingManager::OnProtoDataComplete(
    std::string&& serialized_trace,
    const std::string& scenario_name,
    const std::string& rule_name,
    std::optional<int32_t> rule_value,
    bool privacy_filter_enabled,
    bool is_local_scenario,
    bool force_upload,
    const base::Token& uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    } else if (is_local_scenario) {
      skip_reason = SkipUploadReason::kLocalScenario;
    } else if (serialized_trace.size() > upload_limit_kb_ * 1024) {
      skip_reason = SkipUploadReason::kSizeLimitExceeded;
    }
    bool should_save_trace = ShouldSaveUnuploadedTrace();
    if (skip_reason != SkipUploadReason::kNoSkip && !should_save_trace) {
      return;
    }
    RecordMetric(Metrics::FINALIZATION_STARTED);

    BaseTraceReport base_report;
    base_report.uuid = uuid;
    base_report.creation_time = base::Time::Now();
    base_report.scenario_name = scenario_name;
    base_report.upload_rule_name = rule_name;
    base_report.upload_rule_value = rule_value;
    base_report.total_size = serialized_trace.size();
    base_report.skip_reason = skip_reason;

    std::string serialized_system_profile =
        RecordSerializedSystemProfileMetrics();

    database_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            AddTraceOnDatabaseTaskRunner,
            base::Unretained(trace_database_.get()),
            std::move(serialized_trace), std::move(serialized_system_profile),
            std::move(base_report), should_save_trace, force_upload,
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::BindOnce(&BackgroundTracingManager::OnTraceSaved,
                           weak_factory_.GetWeakPtr(), scenario_name)));
  } else {
    RecordMetric(Metrics::FINALIZATION_STARTED_WITH_LOCAL_OUTPUT);
    receive_callback_.Run(
        uuid.ToString() + ".perfetto.gz", std::move(serialized_trace),
        base::BindOnce(&BackgroundTracingManager::OnFinalizeComplete,
                       weak_factory_.GetWeakPtr(), std::nullopt));
  }
}

bool BackgroundTracingManager::DoEmitNamedTrigger(
    const std::string& trigger_name,
    std::optional<int32_t> value,
    uint64_t flow_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NotifyObservers(trigger_name, value, flow_id);
}

void BackgroundTracingManager::InvalidateTriggersCallbackForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearObserversForTesting();  // IN-TEST
}

void BackgroundTracingManager::OnStartTracingDone() {
  for (EnabledStateTestObserver* observer : background_tracing_observers_) {
    observer->OnTraceStarted();
  }
}

void BackgroundTracingManager::GenerateMetadataProto(
    perfetto::protos::pbzero::ChromeMetadataPacket* metadata,
    bool privacy_filtering_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (active_scenario_) {
    active_scenario_->GenerateMetadataProto(metadata);
  }
}

void BackgroundTracingManager::AbortScenarioForTesting() {
  if (active_scenario_) {
    active_scenario_->Abort();
  }
}

void BackgroundTracingManager::CleanDatabase() {
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
            trace_database->DeleteUploadedTraceContentOlderThan(
                kUploadedTraceContentTimeToLive);
            return trace_database->GetScenarioCountsSince(
                base::Time::Now() - kMaxTracesPerScenarioDuration);
          },
          base::Unretained(trace_database_.get())),
      base::BindOnce(&BackgroundTracingManager::OnTraceDatabaseUpdated,
                     weak_factory_.GetWeakPtr()));
}

void BackgroundTracingManager::DeleteTracesInDateRange(base::Time start,
                                                       base::Time end) {
  // The trace report database needs to exist for clean up. Avoid creating or
  // initializing the trace report database to perform a database clean up.
  std::optional<base::FilePath> database_dir;
  if (!trace_database_) {
    database_dir = GetLocalTracesDirectory();
    if (database_dir.has_value()) {
      return;
    }
    trace_database_ = {new TraceReportDatabase,
                       base::OnTaskRunnerDeleter(database_task_runner_)};
  }
  auto on_database_updated =
      base::BindOnce(&BackgroundTracingManager::OnTraceDatabaseUpdated,
                     weak_factory_.GetWeakPtr());
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](TraceReportDatabase* trace_database,
             std::optional<base::FilePath> database_dir, base::Time start,
             base::Time end,
             scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
             base::OnceCallback<void(ScenarioCountMap)> on_database_updated) {
            if (database_dir.has_value() &&
                !trace_database->OpenDatabaseIfExists(database_dir.value())) {
              return;
            }
            if (!trace_database->is_initialized()) {
              return;
            }
            if (trace_database->DeleteTracesInDateRange(start, end)) {
              reply_task_runner->PostTask(
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
          base::SequencedTaskRunner::GetCurrentDefault(),
          std::move(on_database_updated)));
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::BackgroundMetadataDataSource);
