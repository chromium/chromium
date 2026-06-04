// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tracing/ios_tracing_controller.h"

#import <UIKit/UIKit.h>

#import "base/files/file_path.h"
#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/path_service.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/trace_event/trace_config.h"
#import "base/trace_event/trace_event.h"
#import "base/tracing/perfetto_platform.h"
#import "components/tracing/common/background_tracing_metrics_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/profile/incognito_session_tracker.h"
#import "services/tracing/public/cpp/perfetto/common_data_sources.h"
#import "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#import "services/tracing/public/cpp/perfetto/perfetto_config.h"
#import "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#import "services/tracing/public/cpp/startup_tracing_controller.h"
#import "services/tracing/public/cpp/trace_startup_config.h"
#import "third_party/metrics_proto/system_profile.pb.h"
#import "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#import "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace {
IOSTracingController* g_instance = nullptr;
}  // namespace

// static
IOSTracingController& IOSTracingController::GetInstance() {
  CHECK(g_instance);
  return *g_instance;
}

// static
bool IOSTracingController::HasInstance() {
  return g_instance != nullptr;
}

// static
void IOSTracingController::CreateInstance() {
  static base::NoDestructor<IOSTracingController> instance;
  instance->Initialize();
}

// static
void IOSTracingController::MaybeCreateInstanceForTesting() {
  static base::NoDestructor<IOSTracingController> instance;
}

IOSTracingController::IOSTracingController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  platform_ = std::make_unique<base::tracing::PerfettoPlatform>(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));

  startup_tracing_controller_ =
      std::make_unique<tracing::StartupTracingController>(
          base::SingleThreadTaskRunner::GetCurrentDefault());
}

IOSTracingController::~IOSTracingController() {
  g_instance = nullptr;
}

void IOSTracingController::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!perfetto::Tracing::IsInitialized());
  perfetto::TracingInitArgs init_args;
  init_args.backends = perfetto::BackendType::kInProcessBackend;
  // Use a reasonable buffer size for tracing (e.g. 10MB).
  init_args.shmem_size_hint_kb = 10 * 1024;
  init_args.shmem_batch_commits_duration_ms = 1000;
  init_args.shmem_direct_patching_enabled = true;
  init_args.disallow_merging_with_system_tracks = true;

  init_args.platform = platform_.get();

  perfetto::Tracing::Initialize(init_args);
  tracing::RegisterCommonPerfettoDataSources(/*enable_consumer=*/true);

  // Start the shared startup tracing controller. It will check if
  // TraceStartupConfig is enabled via command line internally.
  startup_tracing_controller_->StartIfNeeded();

  if (GetApplicationContext() &&
      GetApplicationContext()->GetIncognitoSessionTracker()) {
    incognito_tracker_subscription_ =
        GetApplicationContext()->GetIncognitoSessionTracker()->RegisterCallback(
            base::BindRepeating(
                &IOSTracingController::OnIncognitoSessionStateChanged,
                weak_ptr_factory_.GetWeakPtr()));
  }
}

perfetto::TraceConfig IOSTracingController::CreateDeveloperTraceConfig() {
  base::trace_event::TraceConfig trace_config(
      "*,disabled-by-default-cpu_profiler,disabled-by-default-system_metrics,"
      "disabled-by-default-histogram_samples",
      base::trace_event::RECORD_UNTIL_FULL);
  trace_config.SetTraceBufferSizeInBytes(base::ByteSize(50 * 1024 * 1024));

  return tracing::GetDefaultPerfettoConfig(trace_config,
                                           /*privacy_filtering_enabled=*/false,
                                           /*convert_to_legacy_json=*/false,
                                           /*json_agent_label_filter=*/"");
}

void IOSTracingController::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  startup_tracing_controller_->ShutdownAndWaitForStopIfNeeded();
  DisableScenarios();
  field_scenarios_.clear();
  trace_database_.reset();
  trace_report_to_upload_.reset();
  tracing::TrackNameRecorder::GetInstance()->StopRecording();
  tracing::TraceStartupConfig::ResetForTesting();        // IN-TEST
  platform_->ResetTaskRunner(base::SingleThreadTaskRunner::GetCurrentDefault());
  if (base::ThreadPoolInstance::Get()) {
    base::ThreadPoolInstance::Get()->FlushForTesting();  // IN-TEST
  }
  perfetto::Tracing::ResetForTesting();                  // IN-TEST
  incognito_tracker_subscription_ = {};
  tracing::CustomEventRecorder::GetInstance()->DetachFromSequence();
}

void IOSTracingController::InitializeForTesting() {
  platform_->ResetTaskRunner(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  database_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  Initialize();
  InitializeTraceReportDatabase(/*open_in_memory=*/true);
}

void IOSTracingController::SetLatestIncognitoLaunchedForTesting(  // IN-TEST
    base::TimeTicks timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  latest_incognito_launched_ = timestamp;
}

bool IOSTracingController::GetBackgroundStartupTracingEnabled() const {
  return false;
}

bool IOSTracingController::ShouldSaveUnuploadedTrace() {
  return true;
}

std::string IOSTracingController::RecordSerializedSystemProfileMetrics() {
  std::string serialized_system_profile;
  auto recorder = tracing::BackgroundTracingMetricsProvider::
      GetSystemProfileMetricsRecorder();
  if (recorder) {
    metrics::SystemProfileProto system_profile_proto;
    recorder.Run(system_profile_proto);
    system_profile_proto.SerializeToString(&serialized_system_profile);
  }
  return serialized_system_profile;
}

std::optional<base::FilePath> IOSTracingController::GetLocalTracesDirectory() {
  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    return user_data_dir.Append(FILE_PATH_LITERAL("Local Traces"));
  }
  return std::nullopt;
}

void IOSTracingController::OnIncognitoSessionStateChanged(
    bool has_incognito_tabs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (has_incognito_tabs) {
    latest_incognito_launched_ = base::TimeTicks::Now();
  }
}

bool IOSTracingController::IsRecordingAllowed(
    bool privacy_filter_enabled,
    base::TimeTicks scenario_start_time) {
  if (!privacy_filter_enabled) {
    return true;
  }

  bool incognito_active = false;
  if (GetApplicationContext() &&
      GetApplicationContext()->GetIncognitoSessionTracker()) {
    incognito_active = GetApplicationContext()
                           ->GetIncognitoSessionTracker()
                           ->HasIncognitoSessionTabs();
  }

  if (incognito_active || scenario_start_time <= latest_incognito_launched_) {
    return false;
  }

  return true;
}

void IOSTracingController::MaybeConstructPendingAgents() {}
