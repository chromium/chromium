// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program generates the set of files needed for the crash-
// cache unit tests (DiskCacheTest,CacheBackend_Recover*). This program only
// works properly on debug mode, because the crash functionality is not compiled
// on release builds of the cache.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/rankings.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"

using base::Time;

enum Errors {
  GENERIC = -1,
  ALL_GOOD = 0,
  INVALID_ARGUMENT = 1,
  CRASH_OVERWRITE,
  NOT_REACHED
};

using disk_cache::RankCrashes;

// Starts a new process, to generate the files.
int RunSlave(RankCrashes action) {
  base::FilePath exe;
  base::PathService::Get(base::FILE_EXE, &exe);

  base::CommandLine cmdline(exe);
  cmdline.AppendArg(base::NumberToString(action));

  base::Process process = base::LaunchProcess(cmdline, base::LaunchOptions());
  if (!process.IsValid()) {
    printf("Unable to run test %d\n", action);
    return GENERIC;
  }

  int exit_code;

  if (!process.WaitForExit(&exit_code)) {
    printf("Unable to get return code, test %d\n", action);
    return GENERIC;
  }
  if (ALL_GOOD != exit_code)
    printf("Test %d failed, code %d\n", action, exit_code);

  return exit_code;
}

// Main loop for the master process.
int MasterCode() {
  for (int i = disk_cache::NO_CRASH + 1; i < disk_cache::MAX_CRASH; i++) {
    int ret = RunSlave(static_cast<RankCrashes>(i));
    if (ALL_GOOD != ret)
      return ret;
  }

  return ALL_GOOD;
}

// -----------------------------------------------------------------------

namespace disk_cache {
NET_EXPORT_PRIVATE extern RankCrashes g_rankings_crash;
}

const char kCrashEntryName[] = "the first key";

// Creates the destinaton folder for this run, and returns it on full_path.
bool CreateTargetFolder(const base::FilePath& path, RankCrashes action,
                        base::FilePath* full_path) {
  const char* const folders[] = {
    "",
    "insert_empty1",
    "insert_empty2",
    "insert_empty3",
    "insert_one1",
    "insert_one2",
    "insert_one3",
    "insert_load1",
    "insert_load2",
    "remove_one1",
    "remove_one2",
    "remove_one3",
    "remove_one4",
    "remove_head1",
    "remove_head2",
    "remove_head3",
    "remove_head4",
    "remove_tail1",
    "remove_tail2",
    "remove_tail3",
    "remove_load1",
    "remove_load2",
    "remove_load3"
  };
  static_assert(base::size(folders) == disk_cache::MAX_CRASH, "sync folders");
  DCHECK(action > disk_cache::NO_CRASH && action < disk_cache::MAX_CRASH);

  *full_path = path.AppendASCII(folders[action]);

  if (base::PathExists(*full_path))
    return false;

  return base::CreateDirectory(*full_path);
}

// Makes sure that any pending task is processed.
void FlushQueue(disk_cache::Backend* cache) {
  net::TestCompletionCallback cb;
  int rv =
      reinterpret_cast<disk_cache::BackendImpl*>(cache)->FlushQueueForTest(
          cb.callback());
  cb.GetResult(rv);  // Ignore the result;
}

bool CreateCache(const base::FilePath& path,
                 base::Thread* thread,
                 disk_cache::Backend** cache,
                 net::TestCompletionCallback* cb) {
  int size = 1024 * 1024;
  disk_cache::BackendImpl* backend = new disk_cache::BackendImpl(
      path, /* cleanup_tracker = */ nullptr, thread->task_runner().get(),
      net::DISK_CACHE, /* net_log = */ nullptr);
  backend->SetMaxSize(size);
  backend->SetFlags(disk_cache::kNoRandom);
  int rv = backend->Init(cb->callback());
  *cache = backend;
  return (cb->GetResult(rv) == net::OK && !(*cache)->GetEntryCount());
}

// Generates the files for an empty and one item cache.
int SimpleInsert(const base::FilePath& path, RankCrashes action,
                 base::Thread* cache_thread) {
  net::TestCompletionCallback cb;
  disk_cache::Backend* cache;
  if (!CreateCache(path, cache_thread, &cache, &cb))
    return GENERIC;

  const char* test_name = "some other key";

  if (action <= disk_cache::INSERT_EMPTY_3) {
    test_name = kCrashEntryName;
    disk_cache::g_rankings_crash = action;
  }

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult result = cb_create.GetResult(
      cache->CreateEntry(test_name, net::HIGHEST, cb_create.callback()));
  if (result.net_error() != net::OK)
    return GENERIC;

  result.ReleaseEntry()->Close();
  FlushQueue(cache);

  DCHECK(action <= disk_cache::INSERT_ONE_3);
  disk_cache::g_rankings_crash = action;
  test_name = kCrashEntryName;

  result = cb_create.GetResult(
      cache->CreateEntry(test_name, net::HIGHEST, cb_create.callback()));
  if (result.net_error() != net::OK)
    return GENERIC;

  return NOT_REACHED;
}

// Generates the files for a one item cache, and removing the head.
int SimpleRemove(const base::FilePath& path, RankCrashes action,
                 base::Thread* cache_thread) {
  DCHECK(action >= disk_cache::REMOVE_ONE_1);
  DCHECK(action <= disk_cache::REMOVE_TAIL_3);

  net::TestCompletionCallback cb;
  disk_cache::Backend* cache;
  if (!CreateCache(path, cache_thread, &cache, &cb))
    return GENERIC;

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult result = cb_create.GetResult(
      cache->CreateEntry(kCrashEntryName, net::HIGHEST, cb_create.callback()));
  if (result.net_error() != net::OK)
    return GENERIC;

  result.ReleaseEntry()->Close();
  FlushQueue(cache);

  if (action >= disk_cache::REMOVE_TAIL_1) {
    result = cb_create.GetResult(cache->CreateEntry(
        "some other key", net::HIGHEST, cb_create.callback()));
    if (result.net_error() != net::OK)
      return GENERIC;

    result.ReleaseEntry()->Close();
    FlushQueue(cache);
  }

  result = cb_create.GetResult(
      cache->OpenEntry(kCrashEntryName, net::HIGHEST, cb_create.callback()));
  if (result.net_error() != net::OK)
    return GENERIC;

  disk_cache::g_rankings_crash = action;
  disk_cache::Entry* entry = result.ReleaseEntry();
  entry->Doom();
  entry->Close();
  FlushQueue(cache);

  return NOT_REACHED;
}

int HeadRemove(const base::FilePath& path, RankCrashes action,
               base::Thread* cache_thread) {
  DCHECK(action >= disk_cache::REMOVE_HEAD_1);
  DCHECK(action <= disk_cache::REMOVE_HEAD_4);

  net::TestCompletionCallback cb;
  disk_cache::Backend* cache;
  if (!CreateCache(path, cache_thread, &cache, &cb))
    return GENERIC;

  TestEntryResultCompletionCallback cb_create;
  disk_cache::EntryResult result = cb_create.GetResult(
      cache->CreateEntry("some other key", net::HIGHEST, cb_create.callback()));
  if (result.net_error() != net::OK)
    return GENERIC;

  result.ReleaseEntry()->Close();
  FlushQueue(cache);
  result = cb_create.GetResult(
      cache->CreateEntry(kCrashEntryName, net::HIGHEST, cb_create.callback()));
  if (result.net_error() != net::OK)
    return GENERIC;

  result.ReleaseEntry()->Close();
  FlushQueue(cache);

  result = cb_create.GetResult(
      cache->OpenEntry(kCrashEntryName, net::HIGHEST, cb_create.callback()));
  if (result.net_error() != net::OK)
    return GENERIC;

  disk_cache::g_rankings_crash = action;
  disk_cache::Entry* entry = result.ReleaseEntry();
  entry->Doom();
  entry->Close();
  FlushQueue(cache);

  return NOT_REACHED;
}

// Generates the files for insertion and removals on heavy loaded caches.
int LoadOperations(const base::FilePath& path, RankCrashes action,
                   base::Thread* cache_thread) {
  DCHECK(action >= disk_cache::INSERT_LOAD_1);

  // Work with a tiny index table (16 entries).
  disk_cache::BackendImpl* cache = new disk_cache::BackendImpl(
      path, 0xf, cache_thread->task_runner().get(), net::DISK_CACHE, nullptr);
  if (!cache->SetMaxSize(0x100000))
    return GENERIC;

  // No experiments and use a simple LRU.
  cache->SetFlags(disk_cache::kNoRandom);
  net::TestCompletionCallback cb;
  int rv = cache->Init(cb.callback());
  if (cb.GetResult(rv) != net::OK || cache->GetEntryCount())
    return GENERIC;

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  TestEntryResultCompletionCallback cb_create;
  for (int i = 0; i < 100; i++) {
    std::string key = GenerateKey(true);
    disk_cache::EntryResult result = cb_create.GetResult(
        cache->CreateEntry(key, net::HIGHEST, cb_create.callback()));
    if (result.net_error() != net::OK)
      return GENERIC;
    result.ReleaseEntry()->Close();
    FlushQueue(cache);
    if (50 == i && action >= disk_cache::REMOVE_LOAD_1) {
      result = cb_create.GetResult(cache->CreateEntry(
          kCrashEntryName, net::HIGHEST, cb_create.callback()));
      if (result.net_error() != net::OK)
        return GENERIC;
      result.ReleaseEntry()->Close();
      FlushQueue(cache);
    }
  }

  if (action <= disk_cache::INSERT_LOAD_2) {
    disk_cache::g_rankings_crash = action;

    disk_cache::EntryResult result = cb_create.GetResult(cache->CreateEntry(
        kCrashEntryName, net::HIGHEST, cb_create.callback()));
    if (result.net_error() != net::OK)
      return GENERIC;
    result.ReleaseEntry();  // leaks.
  }

  disk_cache::EntryResult result = cb_create.GetResult(
      cache->OpenEntry(kCrashEntryName, net::HIGHEST, cb_create.callback()));
  if (result.net_error() != net::OK)
    return GENERIC;

  disk_cache::g_rankings_crash = action;

  disk_cache::Entry* entry = result.ReleaseEntry();
  entry->Doom();
  entry->Close();
  FlushQueue(cache);

  return NOT_REACHED;
}

// Main function on the child process.
int SlaveCode(const base::FilePath& path, RankCrashes action) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  base::FilePath full_path;
  if (!CreateTargetFolder(path, action, &full_path)) {
    printf("Destination folder found, please remove it.\n");
    return CRASH_OVERWRITE;
  }

  base::Thread cache_thread("CacheThread");
  if (!cache_thread.StartWithOptions(
          base::Thread::Options(base::MessagePumpType::IO, 0)))
    return GENERIC;

  if (action <= disk_cache::INSERT_ONE_3)
    return SimpleInsert(full_path, action, &cache_thread);

  if (action <= disk_cache::INSERT_LOAD_2)
    return LoadOperations(full_path, action, &cache_thread);

  if (action <= disk_cache::REMOVE_ONE_4)
    return SimpleRemove(full_path, action, &cache_thread);

  if (action <= disk_cache::REMOVE_HEAD_4)
    return HeadRemove(full_path, action, &cache_thread);

  if (action <= disk_cache::REMOVE_TAIL_3)
    return SimpleRemove(full_path, action, &cache_thread);

  if (action <= disk_cache::REMOVE_LOAD_3)
    return LoadOperations(full_path, action, &cache_thread);

  return NOT_REACHED;
}

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destructed.
  base::AtExitManager at_exit_manager;

  if (argc < 2)
    return MasterCode();

  char* end;
  RankCrashes action = static_cast<RankCrashes>(strtol(argv[1], &end, 0));
  if (action <= disk_cache::NO_CRASH || action >= disk_cache::MAX_CRASH) {
    printf("Invalid action\n");
    return INVALID_ARGUMENT;
  }

  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("cache_tests");
  path = path.AppendASCII("new_crashes");

  return SlaveCode(path, action);
}
