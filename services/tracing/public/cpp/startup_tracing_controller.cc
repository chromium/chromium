// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/startup_tracing_controller.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#if !BUILDFLAG(IS_IOS)
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#endif
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

#if BUILDFLAG(IS_IOS)
#include "base/apple/foundation_util.h"
#endif

namespace tracing {
namespace {

StartupTracingController::TempFilePolicy g_temp_file_policy =
#if BUILDFLAG(IS_IOS)
    StartupTracingController::TempFilePolicy::kWriteDirectly;
#else
    StartupTracingController::TempFilePolicy::kUseTemporaryFile;
#endif

std::string& GetGlobalDefaultBasename() {
  static base::NoDestructor<std::string> basename;
  return *basename;
}

bool& GetGlobalBasenameForTestSet() {
  static bool basename_for_test_set = false;
  return basename_for_test_set;
}

}  // namespace

class StartupTracingController::BackgroundTracer {
 public:
  enum class WriteMode { kAfterStopping, kStreaming };

  BackgroundTracer(WriteMode write_mode,
                   TempFilePolicy temp_file_policy,
                   base::FilePath output_file,
                   tracing::TraceStartupConfig::OutputFormat output_format,
                   perfetto::TraceConfig trace_config,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   base::OnceClosure default_finished_callback)
      : state_(State::kTracing),
        write_mode_(write_mode),
        temp_file_policy_(temp_file_policy),
        task_runner_(std::move(task_runner)),
        output_file_(output_file),
        output_format_(output_format),
        on_tracing_finished_(std::move(default_finished_callback)) {
#if BUILDFLAG(IS_IOS)
    tracing_session_ =
        perfetto::Tracing::NewTrace(perfetto::BackendType::kInProcessBackend);
#else
    tracing_session_ =
        perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
#endif

    if (write_mode_ == WriteMode::kStreaming) {
#if !BUILDFLAG(IS_WIN)
      OpenFile(output_file_);
      tracing_session_->Setup(trace_config, file_.TakePlatformFile());
#else
      NOTREACHED() << "Streaming to file is not supported on Windows yet";
#endif
    } else {
      tracing_session_->Setup(trace_config);
    }

    tracing_session_->SetOnStopCallback(
        [task_runner = task_runner_,
         weak_ptr = weak_ptr_factory_.GetWeakPtr()]() {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(&BackgroundTracer::OnTracingStopped, weak_ptr));
        });

    tracing_session_->StartBlocking();

    TRACE_EVENT("startup", "StartupTracingController::Start");
  }

 public:
  void Stop(base::OnceClosure on_finished_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!on_finished_callback.is_null()) {
      on_tracing_finished_ = std::move(on_finished_callback);
    }
    if (state_ == State::kFinished) {
      if (on_tracing_finished_) {
        std::move(on_tracing_finished_).Run();
      }
    } else if (state_ == State::kTracing) {
      state_ = State::kStopping;

      tracing_session_->Stop();
    }
  }

  void OnTracingStopped() {
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
    state_ = State::kFinalizing;

    OpenFile(output_file_);

    // The owner of BackgroundTracer is responsible for ensuring that
    // BackgroundTracer stays alive until |on_tracing_finished_| is called.
    tracing_session_->ReadTrace(
        [this](perfetto::TracingSession::ReadTraceCallbackArgs args) {
          WriteData(args.data, args.size);

          if (args.has_more) {
            return;
          }

          Finalise();
        });
  }

 private:
  void WriteData(const char* data, size_t size) {
    // Last chunk can be empty.
    if (size == 0) {
      return;
    }

    base::span<const uint8_t> data_span =
        UNSAFE_TODO(base::as_bytes(base::span(data, size)));

    // Proto files should be written directly to the file.
    if (output_format_ == tracing::TraceStartupConfig::OutputFormat::kProto) {
      file_.WriteAtCurrentPosAndCheck(data_span);
      return;
    }

#if !BUILDFLAG(IS_IOS)
    // For JSON, we need to extract raw data from the packet.
    if (!trace_packet_tokenizer_) {
      trace_packet_tokenizer_ =
          std::make_unique<tracing::TracePacketTokenizer>();
    }

    std::vector<perfetto::TracePacket> packets =
        trace_packet_tokenizer_->Parse(data_span);
    for (const auto& packet : packets) {
      for (const auto& slice : packet.slices()) {
        file_.WriteAtCurrentPosAndCheck(UNSAFE_TODO(base::span(
            reinterpret_cast<const uint8_t*>(slice.start), slice.size)));
      }
    }
#else
    NOTREACHED() << "JSON output is not supported on iOS";
#endif
  }

  // Open |file_| for writing and set |written_to_file_| accordingly.
  // In order to atomically commit the trace file, create a temporary file first
  // which then will be subsequently renamed.
 private:
  void OpenFile(const base::FilePath& path) {
    if (temp_file_policy_ == TempFilePolicy::kUseTemporaryFile) {
      file_ = base::CreateAndOpenTemporaryFileInDir(path.DirName(),
                                                    &written_to_file_);
      if (file_.IsValid()) {
        return;
      }

      VLOG(1) << "Failed to create temporary file, using file '" << path
              << "' directly instead";
    }

    // On Android, it might not be possible to create a temporary file.
    // In that case, we should use the file directly.
    file_.Initialize(output_file_,
                     base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    written_to_file_ = output_file_;

    if (!file_.IsValid()) {
      LOG(ERROR) << "Startup tracing failed: couldn't open file: " << path;
    }
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

    state_ = State::kFinished;
    if (on_tracing_finished_) {
      std::move(on_tracing_finished_).Run();
    }
  }

  enum class State {
    kTracing,
    kStopping,
    kFinalizing,
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
#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<tracing::TracePacketTokenizer> trace_packet_tokenizer_;
#endif

  base::OnceClosure on_tracing_finished_;

  std::unique_ptr<perfetto::TracingSession> tracing_session_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BackgroundTracer> weak_ptr_factory_{this};
};

// A helper class responsible for coordinating emergency trace finalisation
// (e.g. when the process is about to be killed), which can be initiated from
// any thread.
class TracingSessionCoordinator {
 public:
  static TracingSessionCoordinator& GetInstance() {
    static base::NoDestructor<TracingSessionCoordinator> g_instance;
    return *g_instance;
  }

  void SetTracer(
      base::SequenceBound<StartupTracingController::BackgroundTracer> tracer,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
    base::AutoLock lock(lock_);
    CHECK(tracer_.is_null());
    tracer_ = std::move(tracer);
    io_task_runner_ = std::move(io_task_runner);
    background_task_runner_ = std::move(background_task_runner);
    event_.Reset();
  }

  void EmergencyStop() {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    auto tracer = TakeTracer();
    if (!tracer) {
      event_.Wait();
      return;
    }

    tracer.AsyncCall(&StartupTracingController::BackgroundTracer::Stop)
        .WithArgs(base::BindOnce(
            [](TracingSessionCoordinator* coordinator) {
              coordinator->event_.Signal();
            },
            base::Unretained(this)));

    if (io_task_runner_ && io_task_runner_->RunsTasksInCurrentSequence()) {
      VLOG(0)
          << "Emergency tracing stop request from IO thread is ignored - not "
             "possible to finalise trace without running tasks on IO thread";
      return;
    }

    if (background_task_runner_->RunsTasksInCurrentSequence()) {
      VLOG(0) << "Ignored an emergency tracing stop request from the "
                 "StartupTracingController sequence";
      return;
    }

    event_.Wait();
  }

  void ShutdownAndWait() {
    auto tracer = TakeTracer();
    if (!tracer) {
      return;
    }

    base::RunLoop run_loop;
    tracer.AsyncCall(&StartupTracingController::BackgroundTracer::Stop)
        .WithArgs(run_loop.QuitClosure());
    run_loop.Run();

    event_.Signal();
  }

  void OnAutoStopped() {
    auto tracer = TakeTracer();
    if (!tracer) {
      return;
    }
    event_.Signal();
  }

 private:
  base::SequenceBound<StartupTracingController::BackgroundTracer> TakeTracer() {
    base::AutoLock lock(lock_);
    return std::move(tracer_);
  }

  base::Lock lock_;
  base::SequenceBound<StartupTracingController::BackgroundTracer> tracer_
      GUARDED_BY(lock_);

  // Safe to read without lock if we have successfully taken tracer_.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  base::WaitableEvent event_{base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::SIGNALED};
};

base::FilePath StartupTracingController::BasenameToPath(
    std::string_view basename) {
#if BUILDFLAG(IS_ANDROID)
  if (android_path_generator_callback_) {
    return android_path_generator_callback_.Run(basename);
  }
#endif
#if BUILDFLAG(IS_IOS)
  return base::apple::GetUserDocumentPath().AppendASCII(basename);
#else
  // Default to saving the startup trace into the current dir.
  return base::FilePath().AppendASCII(basename);
#endif
}

StartupTracingController::StartupTracingController(
#if BUILDFLAG(IS_ANDROID)
    AndroidPathGeneratorCallback android_path_generator_callback,
#endif
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    :
#if BUILDFLAG(IS_ANDROID)
      android_path_generator_callback_(
          std::move(android_path_generator_callback)),
#endif
      io_task_runner_(std::move(io_task_runner)) {
}

StartupTracingController::~StartupTracingController() {
  CHECK_NE(state_, State::kRunning);
}

base::FilePath StartupTracingController::GetOutputPath() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  base::FilePath path_from_config =
      tracing::TraceStartupConfig::GetInstance().GetResultFile();
  if (!path_from_config.empty()) {
    return path_from_config;
  }

  // If --trace-startup-file is specified, use it.
  if (command_line->HasSwitch(switches::kTraceStartupFile)) {
    base::FilePath result =
        command_line->GetSwitchValuePath(switches::kTraceStartupFile);
    if (result.empty()) {
      return BasenameToPath("chrometrace.log");
    }
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
  if (!result.empty() && !result.EndsWithSeparator()) {
    return result;
  }

  std::string_view basename = GetGlobalDefaultBasename();
  if (basename.empty()) {
    basename = "chrometrace.log";
  }

  // If a non-empty directory is specified, use it.
  if (!result.empty()) {
    return result.AppendASCII(basename);
  }

  // If the directory is empty, go through BasenameToPath to generate a valid
  // path on Android.
  return BasenameToPath(basename);
}

void StartupTracingController::StartIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  base::OnceClosure default_finished = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&StartupTracingController::OnAutoStopped,
                     base::Unretained(this)));

  auto tracer = base::SequenceBound<BackgroundTracer>(
      background_task_runner, write_mode, g_temp_file_policy, GetOutputPath(),
      output_format, perfetto_config, background_task_runner,
      std::move(default_finished));

  TracingSessionCoordinator::GetInstance().SetTracer(
      std::move(tracer), io_task_runner_, background_task_runner);
}

void StartupTracingController::OnAutoStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TracingSessionCoordinator::GetInstance().OnAutoStopped();
  state_ = State::kStopped;
  tracing::TraceStartupConfig::GetInstance().SetDisabled();
}

// static
void StartupTracingController::SetUsingTemporaryFile(
    StartupTracingController::TempFilePolicy temp_file_policy) {
  g_temp_file_policy = temp_file_policy;
}

// static
void StartupTracingController::SetDefaultBasename(
    std::string basename,
    ExtensionType extension_type) {
  if (GetGlobalBasenameForTestSet()) {
    return;
  }

  SetDefaultBasenameInternal(std::move(basename), extension_type);
}

// static
void StartupTracingController::OverrideDefaultBasenameForTest(
    std::string basename,
    ExtensionType extension_type) {
  GetGlobalBasenameForTestSet() = true;
  SetDefaultBasenameInternal(std::move(basename), extension_type);
}

// static
void StartupTracingController::SetDefaultBasenameInternal(
    std::string basename,
    ExtensionType extension_type) {
  if (!tracing::TraceStartupConfig::GetInstance().IsEnabled()) {
    return;
  }

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
  GetGlobalDefaultBasename() = std::move(basename);
}

void StartupTracingController::ShutdownAndWaitForStopIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TracingSessionCoordinator::GetInstance().ShutdownAndWait();

  state_ = State::kStopped;
  tracing::TraceStartupConfig::GetInstance().SetDisabled();
}

// static
void StartupTracingController::EmergencyStop() {
  TracingSessionCoordinator::GetInstance().EmergencyStop();
}

}  // namespace tracing
