// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/startup_tracing_controller.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

#if defined(OS_ANDROID)
#include "content/browser/android/tracing_controller_android.h"
#endif  // defined(OS_ANDROID)

namespace content {

class StartupTracingController::BackgroundTracer {
 public:
  enum class WriteMode { kAfterStopping, kStreaming };

  BackgroundTracer(WriteMode write_mode,
                   base::FilePath output_file,
                   tracing::TraceStartupConfig::OutputFormat output_format,
                   perfetto::TraceConfig trace_config,
                   base::OnceClosure on_tracing_finished)
      : state_(State::kTracing),
        write_mode_(write_mode),
        task_runner_(base::SequencedTaskRunnerHandle::Get()),
        output_file_(output_file),
        output_format_(output_format),
        on_tracing_finished_(std::move(on_tracing_finished)) {
    tracing_session_ = perfetto::Tracing::NewTrace();

    if (write_mode_ == WriteMode::kStreaming) {
#if !defined(OS_WIN)
      OpenFile(output_file_);
      tracing_session_->Setup(trace_config, file_.TakePlatformFile());
#else
      NOTREACHED() << "Streaming to file is not supported on Windows yet";
#endif
    } else {
      tracing_session_->Setup(trace_config);
    }

    tracing_session_->StartBlocking();
    tracing_session_->SetOnStopCallback([&]() { OnTracingStopped(); });

    TRACE_EVENT("startup", "StartupTracingController::Start");
  }

  void Stop() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (state_ != State::kTracing)
      return;

    tracing_session_->StopBlocking();
  }

  void OnTracingStopped() {
    if (!task_runner_->RunsTasksInCurrentSequence()) {
      // The owner of BackgroundTracer is responsible for ensuring that
      // BackgroundTracer stays alive until |on_tracing_finished_| is called.
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&BackgroundTracer::OnTracingStopped,
                                            base::Unretained(this)));
      return;
    }

    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(state_, State::kTracing);
    if (write_mode_ == WriteMode::kStreaming) {
      // No need to explicitly call ReadTrace as Perfetto has already written
      // the file.
      Finalise(output_file_);
      return;
    }
    state_ = State::kWritingToFile;

    OpenFile(output_file_);

    // The owner of BackgroundTracer is responsible for ensuring that
    // BackgroundTracer stays alive until |on_tracing_finished_| is called.
    tracing_session_->ReadTrace(
        [this](perfetto::TracingSession::ReadTraceCallbackArgs args) {
          WriteData(args.data, args.size);

          if (args.has_more)
            return;

          Finalise(output_file_);
        });
  }

 private:
  void WriteData(const char* data, size_t size) {
    // Last chunk can be empty.
    if (size == 0)
      return;

    // Proto files should be written directly to the file.
    if (output_format_ == tracing::TraceStartupConfig::OutputFormat::kProto) {
      file_.WriteAtCurrentPos(data, size);
      return;
    }

    // For JSON, we need to extract raw data from the packet.
    if (!trace_packet_tokenizer_) {
      trace_packet_tokenizer_ =
          std::make_unique<tracing::TracePacketTokenizer>();
    }

    std::vector<perfetto::TracePacket> packets = trace_packet_tokenizer_->Parse(
        reinterpret_cast<const uint8_t*>(data), size);
    for (const auto& packet : packets) {
      for (const auto& slice : packet.slices()) {
        file_.WriteAtCurrentPos(reinterpret_cast<const char*>(slice.start),
                                slice.size);
      }
    }
  }

  // Open |file_| for writing and set |written_to_file_| accordingly.
  // In order to atomically commit the trace file, create a temporary file first
  // which then will be subsequently renamed.
  void OpenFile(const base::FilePath& path) {
    file_ = base::CreateAndOpenTemporaryFileInDir(path.DirName(),
                                                  &written_to_file_);
    if (file_.IsValid()) {
      LOG(ERROR) << "Created valid file";
      return;
    }

    VLOG(1) << "Failed to create temporary file, using file '" << path
            << "' directly instead";

    // On Android, it might not be possible to create a temporary file.
    // In that case, we should use the file directly.
    file_.Initialize(output_file_,
                     base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    written_to_file_ = output_file_;

    if (!file_.IsValid())
      LOG(ERROR) << "Startup tracing failed: couldn't open file: " << path;
  }

  // Close the file and rename if needed.
  void Finalise(const base::FilePath& path) {
    DCHECK_NE(state_, State::kFinished);
    file_.Close();

    if (written_to_file_ != path) {
      base::File::Error error;
      if (!base::ReplaceFile(written_to_file_, output_file_, &error)) {
        LOG(ERROR) << "Cannot move file '" << written_to_file_ << "' to '"
                   << output_file_
                   << "' : " << base::File::ErrorToString(error);
      }
    }

    state_ = State::kFinished;
    std::move(on_tracing_finished_).Run();

    VLOG(0) << "Completed startup tracing to " << path;
  }

  enum class State {
    kTracing,
    kWritingToFile,
    kFinished,
  };
  State state_;

  const WriteMode write_mode_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::FilePath output_file_;
  base::FilePath written_to_file_;
  base::File file_;

  const tracing::TraceStartupConfig::OutputFormat output_format_;

  // Tokenizer to extract the json data from the data received from the tracing
  // service.
  std::unique_ptr<tracing::TracePacketTokenizer> trace_packet_tokenizer_;

  base::OnceClosure on_tracing_finished_;
  std::unique_ptr<perfetto::TracingSession> tracing_session_;

  SEQUENCE_CHECKER(sequence_checker_);
};

namespace {

base::FilePath GetStartupTraceFileName() {
  base::FilePath trace_file;

  trace_file = tracing::TraceStartupConfig::GetInstance()->GetResultFile();
  if (trace_file.empty()) {
#if defined(OS_ANDROID)
    TracingControllerAndroid::GenerateTracingFilePath(&trace_file);
#else
    // Default to saving the startup trace into the current dir.
    trace_file = base::FilePath().AppendASCII("chrometrace.log");
#endif
  }

  return trace_file;
}

}  // namespace

StartupTracingController::StartupTracingController() = default;
StartupTracingController::~StartupTracingController() = default;

// static
StartupTracingController& StartupTracingController::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static base::NoDestructor<StartupTracingController> g_instance;
  return *g_instance;
}

void StartupTracingController::StartIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(state_, State::kRunning);

  auto* trace_startup_config = tracing::TraceStartupConfig::GetInstance();
  if (!trace_startup_config->AttemptAdoptBySessionOwner(
          tracing::TraceStartupConfig::SessionOwner::kTracingController)) {
    return;
  }

  state_ = State::kRunning;

  // Use USER_VISIBLE priority for the drainer because BEST_EFFORT tasks are not
  // run at startup and we want the trace file to be written soon.
  auto background_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  auto output_format =
      tracing::TraceStartupConfig::GetInstance()->GetOutputFormat();

  BackgroundTracer::WriteMode write_mode;
#if defined(OS_WIN)
  // TODO(crbug.com/1158482/): Perfetto does not (yet) support writing directly
  // to a file on Windows.
  write_mode = BackgroundTracer::WriteMode::kAfterStopping;
#else
  // Only protos can be incrementally written to a file - legacy json needs to
  // go through an additional conversion step after, which requires the entire
  // trace to be available.
  write_mode =
      output_format == tracing::TraceStartupConfig::OutputFormat::kProto
          ? BackgroundTracer::WriteMode::kStreaming
          : BackgroundTracer::WriteMode::kAfterStopping;
#endif

  const auto& chrome_config =
      tracing::TraceStartupConfig::GetInstance()->GetTraceConfig();
  perfetto::TraceConfig perfetto_config = tracing::GetDefaultPerfettoConfig(
      chrome_config, /*privacy_filtering_enabled=*/false,
      /*convert_to_legacy_json=*/output_format ==
          tracing::TraceStartupConfig::OutputFormat::kLegacyJSON);

  int duration_in_seconds =
      tracing::TraceStartupConfig::GetInstance()->GetStartupDuration();
  perfetto_config.set_duration_ms(duration_in_seconds * 1000);

  if (output_file_.empty())
    output_file_ = GetStartupTraceFileName();

  background_tracer_ = base::SequenceBound<BackgroundTracer>(
      std::move(background_task_runner), write_mode, output_file_,
      output_format, perfetto_config,
      base::BindOnce(
          [](StartupTracingController* controller) {
            GetUIThreadTaskRunner({})->PostTask(
                FROM_HERE,
                base::BindOnce(&StartupTracingController::OnStoppedOnUIThread,
                               base::Unretained(controller)));
          },
          this));
}

void StartupTracingController::Stop(base::OnceClosure on_tracing_finished) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (state_ == State::kNotRunning) {
    std::move(on_tracing_finished).Run();
    return;
  }

  DCHECK(!on_tracing_finished_) << "Stop() should be called only once.";
  on_tracing_finished_ = std::move(on_tracing_finished);

  background_tracer_.AsyncCall(&BackgroundTracer::Stop);
}

void StartupTracingController::OnStoppedOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_, State::kRunning);
  state_ = State::kNotRunning;
  background_tracer_.Reset();

  if (on_tracing_finished_)
    std::move(on_tracing_finished_).Run();

  tracing::TraceStartupConfig::GetInstance()->OnTraceToResultFileFinished();
  tracing::TraceStartupConfig::GetInstance()->SetDisabled();
}

void StartupTracingController::WaitUntilStopped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::RunLoop run_loop;
  Stop(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace content
