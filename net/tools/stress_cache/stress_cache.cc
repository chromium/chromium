// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple application that stress-tests the crash recovery of the disk
// cache. The main application starts a copy of itself on a loop, checking the
// exit code of the child process. When the child dies in an unexpected way,
// the main application quits.

// The child application has two threads: one to exercise the cache in an
// infinite loop, and another one to asynchronously kill the process.

// A regular build should never crash.
// To test that the disk cache doesn't generate critical errors with regular
// application level crashes, edit stress_support.h.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/stress_support.h"
#include "net/disk_cache/blockfile/trace.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"

#if defined(OS_WIN)
#include "base/logging_win.h"
#endif

using base::Time;

const int kError = -1;
const int kExpectedCrash = 100;

// Starts a new process.
int RunSlave(int iteration) {
  base::FilePath exe;
  base::PathService::Get(base::FILE_EXE, &exe);

  base::CommandLine cmdline(exe);
  cmdline.AppendArg(base::NumberToString(iteration));

  base::Process process = base::LaunchProcess(cmdline, base::LaunchOptions());
  if (!process.IsValid()) {
    printf("Unable to run test\n");
    return kError;
  }

  int exit_code;
  if (!process.WaitForExit(&exit_code)) {
    printf("Unable to get return code\n");
    return kError;
  }
  return exit_code;
}

// Main loop for the master process.
int MasterCode() {
  for (int i = 0; i < 100000; i++) {
    int ret = RunSlave(i);
    if (kExpectedCrash != ret)
      return ret;
  }

  printf("More than enough...\n");

  return 0;
}

// -----------------------------------------------------------------------

std::string GenerateStressKey() {
  char key[20 * 1024];
  size_t size = 50 + rand() % 20000;
  CacheTestFillBuffer(key, size, true);

  key[size - 1] = '\0';
  return std::string(key);
}

// kNumKeys is meant to be enough to have about 3x or 4x iterations before
// the process crashes.
#ifdef NDEBUG
const int kNumKeys = 4000;
#else
const int kNumKeys = 1200;
#endif
const int kNumEntries = 30;
const int kBufferSize = 2000;
const int kReadSize = 20;

// Things that an entry can be doing.
enum Operation { NONE, OPEN, CREATE, READ, WRITE, DOOM };

// This class encapsulates a cache entry and the operations performed on that
// entry. An entry is opened or created as needed, the current content is then
// verified and then something is written to the entry. At that point, the
// |state_| becomes NONE again, waiting for another write, unless the entry is
// closed or deleted.
class EntryWrapper {
 public:
  EntryWrapper() : entry_(nullptr), state_(NONE) {
    buffer_ = base::MakeRefCounted<net::IOBuffer>(kBufferSize);
    memset(buffer_->data(), 'k', kBufferSize);
  }

  Operation state() const { return state_; }

  void DoOpen(int key);

 private:
  void OnOpenDone(int key, disk_cache::EntryResult result);
  void DoRead();
  void OnReadDone(int result);
  void DoWrite();
  void OnWriteDone(int size, int result);
  void DoDelete(const std::string& key);
  void OnDeleteDone(int result);
  void DoIdle();

  disk_cache::Entry* entry_;
  Operation state_;
  scoped_refptr<net::IOBuffer> buffer_;
};

// The data that the main thread is working on.
struct Data {
  Data() : pendig_operations(0), writes(0), iteration(0), cache(nullptr) {}

  int pendig_operations;  // Counter of simultaneous operations.
  int writes;             // How many writes since this iteration started.
  int iteration;          // The iteration (number of crashes).
  disk_cache::BackendImpl* cache;
  std::string keys[kNumKeys];
  EntryWrapper entries[kNumEntries];
};

Data* g_data = nullptr;

void EntryWrapper::DoOpen(int key) {
  DCHECK_EQ(state_, NONE);
  if (entry_)
    return DoRead();

  state_ = OPEN;
  disk_cache::EntryResult result = g_data->cache->OpenEntry(
      g_data->keys[key], net::HIGHEST,
      base::BindOnce(&EntryWrapper::OnOpenDone, base::Unretained(this), key));
  if (result.net_error() != net::ERR_IO_PENDING)
    OnOpenDone(key, std::move(result));
}

void EntryWrapper::OnOpenDone(int key, disk_cache::EntryResult result) {
  if (result.net_error() == net::OK) {
    entry_ = result.ReleaseEntry();
    return DoRead();
  }

  CHECK_EQ(state_, OPEN);
  state_ = CREATE;
  result = g_data->cache->CreateEntry(
      g_data->keys[key], net::HIGHEST,
      base::BindOnce(&EntryWrapper::OnOpenDone, base::Unretained(this), key));
  if (result.net_error() != net::ERR_IO_PENDING)
    OnOpenDone(key, std::move(result));
}

void EntryWrapper::DoRead() {
  int current_size = entry_->GetDataSize(0);
  if (!current_size)
    return DoWrite();

  state_ = READ;
  memset(buffer_->data(), 'k', kReadSize);
  int rv = entry_->ReadData(
      0, 0, buffer_.get(), kReadSize,
      base::BindOnce(&EntryWrapper::OnReadDone, base::Unretained(this)));
  if (rv != net::ERR_IO_PENDING)
    OnReadDone(rv);
}

void EntryWrapper::OnReadDone(int result) {
  DCHECK_EQ(state_, READ);
  CHECK_EQ(result, kReadSize);
  CHECK_EQ(0, memcmp(buffer_->data(), "Write: ", 7));
  DoWrite();
}

void EntryWrapper::DoWrite() {
  bool truncate = (rand() % 2 == 0);
  int size = kBufferSize - (rand() % 20) * kBufferSize / 20;
  state_ = WRITE;
  base::snprintf(buffer_->data(), kBufferSize,
                 "Write: %d iter: %d, size: %d, truncate: %d     ",
                 g_data->writes, g_data->iteration, size, truncate ? 1 : 0);
  int rv = entry_->WriteData(
      0, 0, buffer_.get(), size,
      base::BindOnce(&EntryWrapper::OnWriteDone, base::Unretained(this), size),
      truncate);
  if (rv != net::ERR_IO_PENDING)
    OnWriteDone(size, rv);
}

void EntryWrapper::OnWriteDone(int size, int result) {
  DCHECK_EQ(state_, WRITE);
  CHECK_EQ(size, result);
  if (!(g_data->writes++ % 100))
    printf("Entries: %d    \r", g_data->writes);

  int random = rand() % 100;
  std::string key = entry_->GetKey();
  if (random > 90)
    return DoDelete(key);  // 10% delete then close.

  if (random > 60) {  // 20% close.
    entry_->Close();
    entry_ = nullptr;
  }

  if (random > 80)
    return DoDelete(key);  // 10% close then delete.

  DoIdle();  // 60% do another write later.
}

void EntryWrapper::DoDelete(const std::string& key) {
  state_ = DOOM;
  int rv = g_data->cache->DoomEntry(
      key, net::HIGHEST,
      base::BindOnce(&EntryWrapper::OnDeleteDone, base::Unretained(this)));
  if (rv != net::ERR_IO_PENDING)
    OnDeleteDone(rv);
}

void EntryWrapper::OnDeleteDone(int result) {
  DCHECK_EQ(state_, DOOM);
  if (entry_) {
    entry_->Close();
    entry_ = nullptr;
  }
  DoIdle();
}

void LoopTask();

void EntryWrapper::DoIdle() {
  state_ = NONE;
  g_data->pendig_operations--;
  DCHECK(g_data->pendig_operations);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(&LoopTask));
}

// The task that keeps the main thread busy. Whenever an entry becomes idle this
// task is executed again.
void LoopTask() {
  if (g_data->pendig_operations >= kNumEntries)
    return;

  int slot = rand() % kNumEntries;
  if (g_data->entries[slot].state() == NONE) {
    // Each slot will have some keys assigned to it so that the same entry will
    // not be open by two slots, which means that the state is well known at
    // all times.
    int keys_per_entry = kNumKeys / kNumEntries;
    int key = rand() % keys_per_entry + keys_per_entry * slot;
    g_data->pendig_operations++;
    g_data->entries[slot].DoOpen(key);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(&LoopTask));
}

// This thread will loop forever, adding and removing entries from the cache.
// iteration is the current crash cycle, so the entries on the cache are marked
// to know which instance of the application wrote them.
void StressTheCache(int iteration) {
  int cache_size = 0x2000000;  // 32MB.
  uint32_t mask = 0xfff;       // 4096 entries.

  base::FilePath path;
  base::PathService::Get(base::DIR_TEMP, &path);
  path = path.AppendASCII("cache_test_stress");

  base::Thread cache_thread("CacheThread");
  if (!cache_thread.StartWithOptions(
          base::Thread::Options(base::MessagePumpType::IO, 0)))
    return;

  g_data = new Data();
  g_data->iteration = iteration;
  g_data->cache = new disk_cache::BackendImpl(
      path, mask, cache_thread.task_runner().get(), net::DISK_CACHE, nullptr);
  g_data->cache->SetMaxSize(cache_size);
  g_data->cache->SetFlags(disk_cache::kNoLoadProtection);

  net::TestCompletionCallback cb;
  int rv = g_data->cache->Init(cb.callback());

  if (cb.GetResult(rv) != net::OK) {
    printf("Unable to initialize cache.\n");
    return;
  }
  printf("Iteration %d, initial entries: %d\n", iteration,
         g_data->cache->GetEntryCount());

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  for (int i = 0; i < kNumKeys; i++)
    g_data->keys[i] = GenerateStressKey();

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(&LoopTask));
  base::RunLoop().Run();
}

// We want to prevent the timer thread from killing the process while we are
// waiting for the debugger to attach.
bool g_crashing = false;

// RunSoon() and CrashCallback() reference each other, unfortunately.
void RunSoon(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

void CrashCallback() {
  // Keep trying to run.
  RunSoon(base::ThreadTaskRunnerHandle::Get());

  if (g_crashing)
    return;

  if (rand() % 100 > 30) {
    printf("sweet death...\n");

    // Terminate the current process without doing normal process-exit cleanup.
    base::Process::TerminateCurrentProcessImmediately(kExpectedCrash);
  }
}

void RunSoon(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  const base::TimeDelta kTaskDelay = base::TimeDelta::FromSeconds(10);
  task_runner->PostDelayedTask(FROM_HERE, base::BindOnce(&CrashCallback),
                               kTaskDelay);
}

// We leak everything here :)
bool StartCrashThread() {
  base::Thread* thread = new base::Thread("party_crasher");
  if (!thread->Start())
    return false;

  RunSoon(thread->task_runner());
  return true;
}

void CrashHandler(const char* file,
                  int line,
                  const base::StringPiece str,
                  const base::StringPiece stack_trace) {
  g_crashing = true;
  base::debug::BreakDebugger();
}

bool MessageHandler(int severity, const char* file, int line,
                    size_t message_start, const std::string& str) {
  const size_t kMaxMessageLen = 48;
  char message[kMaxMessageLen];
  size_t len = std::min(str.length() - message_start, kMaxMessageLen - 1);

  memcpy(message, str.c_str() + message_start, len);
  message[len] = '\0';
#if !defined(DISK_CACHE_TRACE_TO_LOG)
  disk_cache::Trace("%s", message);
#endif
  return false;
}

// -----------------------------------------------------------------------

#if defined(OS_WIN)
// {B9A153D4-31C3-48e4-9ABF-D54383F14A0D}
const GUID kStressCacheTraceProviderName = {
    0xb9a153d4, 0x31c3, 0x48e4,
        { 0x9a, 0xbf, 0xd5, 0x43, 0x83, 0xf1, 0x4a, 0xd } };
#endif

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destructed.
  base::AtExitManager at_exit_manager;

  if (argc < 2)
    return MasterCode();

  logging::ScopedLogAssertHandler scoped_assert_handler(
      base::Bind(CrashHandler));
  logging::SetLogMessageHandler(MessageHandler);

#if defined(OS_WIN)
  logging::LogEventProvider::Initialize(kStressCacheTraceProviderName);
#else
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);
#endif

  // Some time for the memory manager to flush stuff.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(3));
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  char* end;
  long int iteration = strtol(argv[1], &end, 0);

  if (!StartCrashThread()) {
    printf("failed to start thread\n");
    return kError;
  }

  StressTheCache(iteration);
  return 0;
}
