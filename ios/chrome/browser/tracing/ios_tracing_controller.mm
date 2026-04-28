// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tracing/ios_tracing_controller.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback_helpers.h"
#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/trace_event/trace_config.h"
#import "base/trace_event/trace_event.h"
#import "base/trace_event/trace_event_impl.h"
#import "base/trace_event/trace_log.h"
#import "base/trace_event/trace_session_observer.h"
#import "base/tracing/perfetto_platform.h"
#import "services/tracing/public/cpp/perfetto/common_data_sources.h"
#import "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#import "services/tracing/public/cpp/perfetto/perfetto_config.h"
#import "services/tracing/public/cpp/startup_tracing_controller.h"
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
  tracing::TraceStartupConfig::ResetForTesting();        // IN-TEST
  perfetto::Tracing::ResetForTesting();                  // IN-TEST
  tracing::CustomEventRecorder::GetInstance()->DetachFromSequence();
}

void IOSTracingController::InitializeForTesting() {
  platform_->ResetTaskRunner(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  Initialize();
}
