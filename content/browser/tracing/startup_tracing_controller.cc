// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/startup_tracing_controller.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/tracing_controller_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

// A helper class responsible for coordinating emergency trace finalisation
// (e.g. when the process is about to be killed), which can be initiated from
// any thread.
class EmergencyTraceFinalisationCoordinator {
 public:
  static EmergencyTraceFinalisationCoordinator& GetInstance() {
    static base::NoDestructor<EmergencyTraceFinalisationCoordinator> g_instance;
    return *g_instance;
  }

  void OnTracingStarted(scoped_refptr<base::SequencedTaskRunner> task_runner,
                        base::OnceClosure stop_tracing) {
    tracing_started_.Set();
    base::AutoLock lock(lock_);
    startup_tracing_controller_task_runner_ = std::move(task_runner);
    stop_tracing_ = std::move(stop_tracing);
  }

  void OnTracingStopped() { finalisation_.Signal(); }

  // May be called multiple times per session, e.g. if a second thread
  // encounters a crash after the first.
  void StopAndBlockUntilStopped() {
    // If DCHECK fires before tracing has started, there isn't much for us to
    // do.
    if (!tracing_started_.IsSet())
      return;

    base::trace_event::TraceLog::GetInstance()
        ->SetCurrentThreadBlocksMessageLoop();

    base::OnceClosure stop_tracing;
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    {
      base::AutoLock lock(lock_);
      task_runner = startup_tracing_controller_task_runner_;
      stop_tracing = std::move(stop_tracing_);
    }

    if (task_runner->RunsTasksInCurrentSequence()) {
      VLOG(0) << "Ignored an emergency tracing stop request from the "
                 "StartupTracingController sequence";
      return;
    }

    if (stop_tracing)
      task_runner->PostTask(FROM_HERE, std::move(stop_tracing));

    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;

    // Wait for the tracing to be finished before processing.
    // Note that we should wait even if |stop_tracing| is null — if a second
    // thread hits DCHECK while the first has posted a task and waits for the
    // trace to be written, the second one should wait as well to avoid crashing
    // the process.
    finalisation_.Wait();
  }

 private:
  base::WaitableEvent finalisation_;
  base::AtomicFlag tracing_started_;

  base::Lock lock_;
  scoped_refptr<base::SequencedTaskRunner>
      startup_tracing_controller_task_runner_ GUARDED_BY(lock_);
  base::OnceClosure stop_tracing_ GUARDED_BY(lock_);
};

class StartupTracingController::BackgroundTracer {
 public:
  enum class WriteMode { kAfterStopping, kStreaming };

  BackgroundTracer(WriteMode write_mode,
                   TempFilePolicy temp_file_policy,
                   base::FilePath output_file,
                   tracing::TraceStartupConfig::OutputFormat output_format,
                   perfetto::TraceConfig trace_config,
                   base::OnceClosure on_tracing_finished)
      : state_(State::kTracing),
        write_mode_(write_mode),
        temp_file_policy_(temp_file_policy),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        output_file_(output_file),
        output_format_(output_format),
        on_tracing_finished_(std::move(on_tracing_finished)) {
    tracing_session_ =
        perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);

    if (write_mode_ == WriteMode::kStreaming) {
#if !BUILDFLAG(IS_WIN)
      OpenFile(output_file_);
      tracing_session_->Setup(trace_config, file_.TakePlatformFile());
#else
      NOTREACHED_IN_MIGRATION()
          << "Streaming to file is not supported on Windows yet";
#endif
    } else {
      tracing_session_->Setup(trace_config);
    }

    // |StartBlocking| can take a non-trivial amount of time, so
    // EmergencyTraceFinalisationController should be set up before it to catch
    // DCHECKs early.
    EmergencyTraceFinalisationCoordinator::GetInstance().OnTracingStarted(
        task_runner_,
        base::BindOnce(&BackgroundTracer::Stop, weak_ptr_factory_.GetWeakPtr(),
                       std::nullopt));

    tracing_session_->SetOnStopCallback([&]() { OnTracingStopped(); });
    tracing_session_->StartBlocking();

    TRACE_EVENT("startup", "StartupTracingController::Start");
  }

  void Stop(std::optional<base::FilePath> output_file) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Tracing might have already been finished due to a timeout.
    if (state_ != State::kTracing) {
      // Note: updating output files is not supported together with
      // timeout-based tracing.
      return;
    }
    state_ = State::kStopping;

    if (output_file)
      output_file_ = output_file.value();
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
    // State will be kStopping if Stop() is called and kTracing if tracing
    // finishes due to a timeout.
    DCHECK(state_ == State::kStopping || state_ == State::kTracing);
    if (write_mode_ == WriteMode::kStreaming) {
      // No need to explicitly call ReadTrace as Perfetto has already written
      // the file.
      Finalise();
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

          Finalise();
        });
  }

 private:
  void WriteData(const char* data, size_t size) {
    // Last chunk can be empty.
    if (size == 0)
      return;

    // Proto files should be written directly to the file.
    if (output_format_ == tracing::TraceStartupConfig::OutputFormat::kProto) {
      UNSAFE_TODO(file_.WriteAtCurrentPos(data, size));
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
        UNSAFE_TODO(file_.WriteAtCurrentPos(
            reinterpret_cast<const char*>(slice.start), slice.size));
      }
    }
  }

  // Open |file_| for writing and set |written_to_file_| accordingly.
  // In order to atomically commit the trace file, create a temporary file first
  // which then will be subsequently renamed.
  void OpenFile(const base::FilePath& path) {
    if (temp_file_policy_ == TempFilePolicy::kUseTemporaryFile) {
      file_ = base::CreateAndOpenTemporaryFileInDir(path.DirName(),
                                                    &written_to_file_);
      if (file_.IsValid())
        return;

      VLOG(1) << "Failed to create temporary file, using file '" << path
              << "' directly instead";
    }

    // On Android, it might not be possible to create a temporary file.
    // In that case, we should use the file directly.
    file_.Initialize(output_file_,
                     base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    written_to_file_ = output_file_;

    if (!file_.IsValid())
      LOG(ERROR) << "Startup tracing failed: couldn't open file: " << path;
  }

  // Close the file and rename if needed.
  void Finalise() {
    DCHECK_NE(state_, State::kFinished);
    file_.Close();

    if (written_to_file_ != output_file_) {
      base::File::Error error;
      if (!base::ReplaceFile(written_to_file_, output_file_, &error)) {
        LOG(ERROR) << "Cannot move file '" << written_to_file_ << "' to '"
                   << output_file_
                   << "' : " << base::File::ErrorToString(error);
      } else {
        written_to_file_ = output_file_;
      }
    }

    VLOG(0) << "Completed startup tracing to " << written_to_file_;
    EmergencyTraceFinalisationCoordinator::GetInstance().OnTracingStopped();

    state_ = State::kFinished;
    std::move(on_tracing_finished_).Run();
  }

  enum class State {
    kTracing,
    kStopping,
    kWritingToFile,
    kFinished,
  };
  State state_;

  const WriteMode write_mode_;
  const TempFilePolicy temp_file_policy_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Output file might be customised during the execution (e.g. test result
  // becomes available), which means that if Perfetto has already started
  // streaming the trace, the trace file should be renamed after trace
  // completes.
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

  base::WeakPtrFactory<BackgroundTracer> weak_ptr_factory_{this};
};

// static
StartupTracingController& StartupTracingController::GetInstance() {
  // Note: no DCHECK_CURRENTLY_ON, as it can be called prior to initialisation
  // of BrowserThreads.

  static base::NoDestructor<StartupTracingController> g_instance;
  return *g_instance;
}

namespace {

base::FilePath BasenameToPath(std::string basename) {
#if BUILDFLAG(IS_ANDROID)
  return TracingControllerAndroid::GenerateTracingFilePath(basename);
#else
  // Default to saving the startup trace into the current dir.
  return base::FilePath().AppendASCII(basename);
#endif
}

}  // namespace

StartupTracingController::StartupTracingController() = default;
StartupTracingController::~StartupTracingController() = default;

base::FilePath StartupTracingController::GetOutputPath() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  base::FilePath path_from_config =
      tracing::TraceStartupConfig::GetInstance().GetResultFile();
  if (!path_from_config.empty())
    return path_from_config;

  // If --trace-startup-file is specified, use it.
  if (command_line->HasSwitch(switches::kTraceStartupFile)) {
    base::FilePath result =
        command_line->GetSwitchValuePath(switches::kTraceStartupFile);
    if (result.empty())
      return BasenameToPath("chrometrace.log");
    return result;
  }

  base::FilePath result =
      command_line->GetSwitchValuePath(switches::kEnableTracingOutput);
  if (result.empty() && command_line->HasSwitch(switches::kTraceStartup)) {
    // If --trace-startup is present, return chrometrace.log for backwards
    // compatibility.
    return BasenameToPath("chrometrace.log");
  }

  // If a non-directory path is specified, use it.
  if (!result.empty() && !result.EndsWithSeparator())
    return result;

  std::string basename = default_basename_;
  if (basename.empty())
    basename = "chrometrace.log";

  // If a non-empty directory is specified, use it.
  if (!result.empty())
    return result.AppendASCII(basename);

  // If the directory is empty, go through BasenameToPath to generate a valid
  // path on Android.
  return BasenameToPath(basename);
}

void StartupTracingController::StartIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(state_, State::kRunning);

  auto& trace_startup_config = tracing::TraceStartupConfig::GetInstance();
  if (!trace_startup_config.AttemptAdoptBySessionOwner(
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
      tracing::TraceStartupConfig::GetInstance().GetOutputFormat();

  BackgroundTracer::WriteMode write_mode;
#if BUILDFLAG(IS_WIN)
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

  auto perfetto_config =
      tracing::TraceStartupConfig::GetInstance().GetPerfettoConfig();

  background_tracer_ = base::SequenceBound<BackgroundTracer>(
      std::move(background_task_runner), write_mode, temp_file_policy_,
      GetOutputPath(), output_format, perfetto_config,
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

  if (state_ != State::kRunning) {
    // Both kStopped and kNotRunning are valid states.
    std::move(on_tracing_finished).Run();
    return;
  }

  DCHECK(!on_tracing_finished_) << "Stop() should be called only once.";
  on_tracing_finished_ = std::move(on_tracing_finished);

  background_tracer_.AsyncCall(&BackgroundTracer::Stop)
      .WithArgs(GetOutputPath());
}

void StartupTracingController::OnStoppedOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_, State::kRunning);
  state_ = State::kStopped;
  background_tracer_.Reset();

  if (on_tracing_finished_)
    std::move(on_tracing_finished_).Run();

  tracing::TraceStartupConfig::GetInstance().SetDisabled();
}

void StartupTracingController::SetUsingTemporaryFile(
    StartupTracingController::TempFilePolicy temp_file_policy) {
  DCHECK_EQ(state_, State::kNotEnabled) << "Should be called before Start()";
  temp_file_policy_ = temp_file_policy;
}

void StartupTracingController::SetDefaultBasename(
    std::string basename,
    ExtensionType extension_type) {
  if (!tracing::TraceStartupConfig::GetInstance().IsEnabled()) {
    return;
  }

  if (basename_for_test_set_)
    return;

  if (extension_type == ExtensionType::kAppendAppropriate) {
    switch (tracing::TraceStartupConfig::GetInstance().GetOutputFormat()) {
      case tracing::TraceStartupConfig::OutputFormat::kLegacyJSON:
        basename += ".json";
        break;
      case tracing::TraceStartupConfig::OutputFormat::kProto:
        basename += ".pftrace";
        break;
    }
  }
  default_basename_ = basename;
}

void StartupTracingController::SetDefaultBasenameForTest(
    std::string basename,
    ExtensionType extension_type) {
  basename_for_test_set_ = false;
  SetDefaultBasename(basename, extension_type);
  basename_for_test_set_ = true;
}

void StartupTracingController::WaitUntilStopped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::RunLoop run_loop;
  Stop(run_loop.QuitClosure());
  run_loop.Run();
}

void StartupTracingController::ShutdownAndWaitForStopIfNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (should_continue_on_shutdown_)
    return;

  WaitUntilStopped();
}

// static
void StartupTracingController::EmergencyStop() {
  if (GetIOThreadTaskRunner({})->RunsTasksInCurrentSequence()) {
    VLOG(0) << "Emergency tracing stop request from IO thread is ignored - not "
               "possible to finalise trace without running tasks on IO thread";
    return;
  }

  EmergencyTraceFinalisationCoordinator::GetInstance()
      .StopAndBlockUntilStopped();
}

}  // namespace content
