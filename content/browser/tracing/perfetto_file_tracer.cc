// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/perfetto_file_tracer.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/tracing_service.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"

#if defined(OS_ANDROID)
#include "content/browser/android/tracing_controller_android.h"
#endif

namespace content {

namespace {

constexpr base::TimeDelta kReadBufferIntervalInSeconds =
    base::TimeDelta::FromSeconds(1);

class BackgroundDrainer : public mojo::DataPipeDrainer::Client {
 public:
  BackgroundDrainer() {
    base::FilePath output_file =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kPerfettoOutputFile);

#if defined(OS_ANDROID)
    if (output_file.empty()) {
      TracingControllerAndroid::GenerateTracingFilePath(&output_file);
      VLOG(0) << "Writing to output file: " << output_file.value();
    }
#endif

    file_.Initialize(output_file,
                     base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (!file_.IsValid()) {
      LOG(ERROR) << "Failed to open file: " << output_file;
    }
  }

  void StartDrain(mojo::ScopedDataPipeConsumerHandle consumer_handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    drainer_.reset(new mojo::DataPipeDrainer(this, std::move(consumer_handle)));
  }

 private:
  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!file_.IsValid()) {
      return;
    }

    size_t written =
        file_.WriteAtCurrentPos(static_cast<const char*>(data), num_bytes);
    if (written != num_bytes) {
      LOG(ERROR) << "Failed writing to trace output file: wrote " << written
                 << " out of " << num_bytes << " bytes.";
    }
  }

  // mojo::DataPipeDrainer::Client
  void OnDataComplete() override {}

  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  base::File file_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(BackgroundDrainer);
};

}  // namespace

// static
bool PerfettoFileTracer::ShouldEnable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPerfettoOutputFile);
}

// Use USER_VISIBLE priority for the drainer because BEST_EFFORT tasks are not
// run at startup and we want the trace file to be written soon.
PerfettoFileTracer::PerfettoFileTracer()
    : background_drainer_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  GetTracingService().BindConsumerHost(
      consumer_host_.BindNewPipeAndPassReceiver());

  const auto& chrome_config =
      tracing::TraceStartupConfig::GetInstance()->GetTraceConfig();
  // The output is always proto based. So, enable privacy filtering if argument
  // filter is enabled.
  bool privacy_filtering = chrome_config.IsArgumentFilterEnabled();
  perfetto::TraceConfig trace_config = tracing::GetDefaultPerfettoConfig(
      chrome_config, privacy_filtering,
      /*convert_to_legacy_json=*/false,
      perfetto::protos::gen::ChromeConfig::USER_INITIATED);

  int duration_in_seconds =
      tracing::TraceStartupConfig::GetInstance()->GetStartupDuration();
  trace_config.set_duration_ms(duration_in_seconds * 1000);

  // We just need a single global trace buffer, for our data.
  trace_config.mutable_buffers()->front().set_size_kb(32 * 1024);

  consumer_host_->EnableTracing(
      tracing_session_host_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote(), std::move(trace_config));
  receiver_.set_disconnect_handler(base::BindOnce(
      &PerfettoFileTracer::OnTracingSessionEnded, base::Unretained(this)));
  ReadBuffers();
}

PerfettoFileTracer::~PerfettoFileTracer() = default;

void PerfettoFileTracer::OnTracingEnabled() {}

void PerfettoFileTracer::OnTracingDisabled(bool tracing_succeeded) {
  DCHECK(tracing_succeeded);
  has_been_disabled_ = true;
}

void PerfettoFileTracer::OnNoMorePackets(bool queued_after_disable) {
  DCHECK(consumer_host_);
  // If this callback happens as the result of a ReadBuffers() call
  // which was queued after tracing was disabled, we know there's
  // going to be no more packets coming and we can clean up
  // the |background_drainer_|.
  if (queued_after_disable) {
    consumer_host_.reset();
    background_drainer_.Reset();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PerfettoFileTracer::ReadBuffers,
                     weak_factory_.GetWeakPtr()),
      kReadBufferIntervalInSeconds);
}

void PerfettoFileTracer::ReadBuffers() {
  DCHECK(consumer_host_);

  mojo::DataPipe data_pipe;
  tracing_session_host_->ReadBuffers(
      std::move(data_pipe.producer_handle),
      base::BindOnce(
          [](PerfettoFileTracer* file_tracing, bool queued_after_disable) {
            file_tracing->OnNoMorePackets(queued_after_disable);
          },
          base::Unretained(this), has_been_disabled_));

  background_drainer_.AsyncCall(&BackgroundDrainer::StartDrain)
      .WithArgs(std::move(data_pipe.consumer_handle));
}

void PerfettoFileTracer::OnTracingSessionEnded() {
  receiver_.reset();
  tracing_session_host_.reset();
}

}  // namespace content
