// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tracing/test_utils.h"

#import <utility>

#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/tracing/perfetto_platform.h"
#import "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#import "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#import "services/tracing/public/cpp/startup_tracing_controller.h"
#import "third_party/perfetto/include/perfetto/tracing/tracing.h"

IOSTracingControllerForTesting::IOSTracingControllerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : IOSTracingController(task_runner), startup_config_() {
  Init(std::move(task_runner));
}

IOSTracingControllerForTesting::IOSTracingControllerForTesting(
    const base::CommandLine& command_line,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : IOSTracingController(task_runner), startup_config_(command_line) {
  Init(std::move(task_runner));
}

void IOSTracingControllerForTesting::Init(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (!task_runner) {
    task_runner =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  }
  base::tracing::PerfettoPlatform::Get().SetupForTesting(
      std::move(task_runner));
  Initialize();
  InitializeTraceReportDatabase(/*open_in_memory=*/true);
}

IOSTracingControllerForTesting::~IOSTracingControllerForTesting() {
  startup_tracing_controller()->ShutdownAndWaitForStopIfNeeded();
  DisableScenarios();
  field_scenarios_.clear();
  trace_database_.reset();
  trace_report_to_upload_.reset();
  tracing::TrackNameRecorder::GetInstance()->StopRecording();
  if (base::ThreadPoolInstance::Get()) {
    base::ThreadPoolInstance::Get()->FlushForTesting();  // IN-TEST
  }
  perfetto::Tracing::ResetForTesting();  // IN-TEST
  tracing::CustomEventRecorder::GetInstance()->DetachFromSequence();
}
