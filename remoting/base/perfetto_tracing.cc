// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/perfetto_tracing.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/perfetto_platform.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.gen.h"
#include "third_party/webrtc_overrides/init_webrtc.h"

namespace remoting {

namespace {

void OnPerfettoLogMessage(perfetto::base::LogMessageCallbackArgs args) {
  // Perfetto log levels start at kLogDebug (0), whereas Chromium's LogSeverity
  // starts at LOGGING_VERBOSE (-1) / LOGGING_INFO (0).
  const int severity = static_cast<int>(args.level) - 1;
  if (!logging::ShouldCreateLogMessage(severity)) {
    return;
  }
  ::logging::LogMessage(args.filename, args.line, severity).stream()
      << args.message;
}

void SetProcessTrackDescriptor(std::string_view process_name) {
  DCHECK(!process_name.empty());
  const auto process_track = perfetto::ProcessTrack::Current();
  // Serialize the base `perfetto::Track` rather than `ProcessTrack` to avoid
  // reading `/proc/self/cmdline` (which fails inside sandboxes and would record
  // command-line switches in trace files).
  perfetto::protos::gen::TrackDescriptor desc =
      static_cast<const perfetto::Track&>(process_track).Serialize();
  desc.set_disallow_merging_with_system_tracks(true);
  auto* process = desc.mutable_process();
  process->set_pid(static_cast<int32_t>(base::GetCurrentProcId()));
  if (!process_name.empty()) {
    process->set_process_name(std::string(process_name));
  }
  base::TrackEvent::SetTrackDescriptor(process_track, std::move(desc));
}

void SetThreadTrackDescriptor(const perfetto::ThreadTrack& track,
                              std::string_view thread_name) {
  if (thread_name.empty()) {
    return;
  }
  perfetto::protos::gen::TrackDescriptor desc =
      static_cast<const perfetto::Track&>(track).Serialize();
  if (track.disallow_merging_with_system_tracks) {
    desc.set_disallow_merging_with_system_tracks(true);
  }
  auto* thread = desc.mutable_thread();
  thread->set_pid(static_cast<int32_t>(base::GetCurrentProcId()));
  thread->set_tid(static_cast<int32_t>(track.tid));
  thread->set_thread_name(std::string(thread_name));
  base::TrackEvent::SetTrackDescriptor(track, std::move(desc));
}

class ThreadNameObserver : public base::ThreadIdNameManager::Observer {
 public:
  static ThreadNameObserver* GetInstance();

  void Register();
  void Unregister();

  // base::ThreadIdNameManager::Observer:
  void OnThreadNameChanged(const char* name) override;

 private:
  friend class base::NoDestructor<ThreadNameObserver>;

  ThreadNameObserver() = default;
  ~ThreadNameObserver() override = default;

  base::Lock lock_;
  bool registered_ GUARDED_BY(lock_) = false;
};

// static
ThreadNameObserver* ThreadNameObserver::GetInstance() {
  static base::NoDestructor<ThreadNameObserver> instance;
  return instance.get();
}

void ThreadNameObserver::Register() {
  bool should_snapshot_existing_threads = false;
  {
    base::AutoLock lock(lock_);
    if (!registered_) {
      registered_ = true;
      should_snapshot_existing_threads = true;
    }
  }
  if (!should_snapshot_existing_threads) {
    return;
  }

  auto* manager = base::ThreadIdNameManager::GetInstance();
  manager->AddObserver(this);

  // Snapshot threads created before `InitPerfettoTracing()` ran (plus the
  // calling thread, which `GetIds()` omits when called on the main thread).
  // Done only once so subsequent trace sessions do not overwrite custom track
  // names set via `SetCurrentThreadTrackName()`.
  for (base::PlatformThreadId tid : manager->GetIds()) {
    const char* name = manager->GetName(tid);
    if (name && name[0] != '\0') {
      SetThreadTrackDescriptor(perfetto::ThreadTrack::ForThread(tid.raw()),
                               name);
    }
  }
  const char* current_name = manager->GetNameForCurrentThread();
  if (current_name && current_name[0] != '\0') {
    SetThreadTrackDescriptor(perfetto::ThreadTrack::Current(), current_name);
  }
}

void ThreadNameObserver::Unregister() {
  bool was_registered = false;
  {
    base::AutoLock lock(lock_);
    if (registered_) {
      registered_ = false;
      was_registered = true;
    }
  }
  if (was_registered) {
    base::ThreadIdNameManager::GetInstance()->RemoveObserver(this);
  }
}

void ThreadNameObserver::OnThreadNameChanged(const char* name) {
  // `ThreadIdNameManager` holds its internal lock here; do not call back into
  // `ThreadIdNameManager`.
  if (name && name[0] != '\0' && perfetto::Tracing::IsInitialized()) {
    SetThreadTrackDescriptor(perfetto::ThreadTrack::Current(), name);
  }
}

}  // namespace

void InitPerfettoTracing(std::string_view process_name) {
  {
    static base::NoDestructor<base::Lock> init_lock;
    base::AutoLock lock(*init_lock);
    if (!perfetto::Tracing::IsInitialized()) {
      auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING});
      base::tracing::PerfettoPlatform::Options platform_options;
      platform_options.real_process_id = base::GetCurrentProcId();
      auto& platform = base::tracing::PerfettoPlatform::MaybeCreateInstance(
          task_runner, std::move(platform_options));
      if (platform.task_runner() != task_runner) {
        platform.ResetTaskRunner(task_runner);
      }

      perfetto::TracingInitArgs init_args;
      init_args.backends = perfetto::kInProcessBackend;
      init_args.platform = &platform;
      init_args.shmem_size_hint_kb = 4 * 1024;
      init_args.shmem_batch_commits_duration_ms = 1000;
      init_args.shmem_direct_patching_enabled = true;
      init_args.use_monotonic_clock = true;
      init_args.disallow_merging_with_system_tracks = true;
      // Generate an explicit non-zero UUID to avoid reading `/proc/self/stat`
      // in `TrackRegistry::ComputeProcessUuid()`.
      uint64_t process_uuid = 0;
      while (process_uuid == 0) {
        process_uuid = base::RandUint64();
      }
      init_args.process_uuid = process_uuid;
      init_args.log_message_callback = &OnPerfettoLogMessage;

      perfetto::Tracing::Initialize(init_args);
    }
  }

  base::TrackEvent::Register();
  InitializeWebRtcModule();

  SetProcessTrackDescriptor(process_name);
  ThreadNameObserver::GetInstance()->Register();
}

void SetCurrentThreadTrackName(std::string_view thread_name) {
  if (!perfetto::Tracing::IsInitialized()) {
    return;
  }
  SetThreadTrackDescriptor(perfetto::ThreadTrack::Current(), thread_name);
}

uint64_t NextPerfettoFrameTraceId() {
  static const uint64_t kProcessPrefix = []() {
    uint64_t prefix = 0;
    while (prefix == 0) {
      prefix = base::RandUint64() & 0xFFFFFFFF00000000ULL;
    }
    return prefix;
  }();
  static std::atomic<uint32_t> next_counter{1};
  return kProcessPrefix | next_counter.fetch_add(1, std::memory_order_relaxed);
}

void ResetPerfettoTracingForTesting() {
  ThreadNameObserver::GetInstance()->Unregister();
  perfetto::Tracing::ResetForTesting();  // IN-TEST
}

}  // namespace remoting
