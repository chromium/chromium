// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/profiling_utils.h"

#include <limits>
#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/clang_profiling_buildflags.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

namespace content {

namespace {

// Returns the path where the PGO profiles should be saved.
// On Android this is always a static path, on other platforms it's either
// the path specified by the LLVM_PROFILE_FILE environment variable or the
// current path if it's not set.
base::FilePath GetProfileFileDirectory() {
  base::FilePath path;

  // Android differs from the other platforms because it's not possible to
  // write in base::DIR_CURRENT and environment variables aren't well supported.
#if defined(OS_ANDROID)
  base::PathService::Get(base::DIR_TEMP, &path);
  path = path.Append("pgo_profiles/");
#else  // !defined(OS_ANDROID)
  std::string prof_template;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->GetVar("LLVM_PROFILE_FILE", &prof_template)) {
#if defined(OS_WIN)
    path = base::FilePath(base::UTF8ToWide(prof_template)).DirName();
#else
    path = base::FilePath(prof_template).DirName();
#endif
  }
#endif

  if (!path.empty()) {
    base::CreateDirectory(path);
  } else {
    base::PathService::Get(base::DIR_CURRENT, &path);
  }

  return path;
}

}  // namespace

base::File OpenProfilingFile() {
  base::ScopedAllowBlockingForTesting allows_blocking;
  base::FilePath path = GetProfileFileDirectory();

  // sajjadm@ and liaoyuke@ experimentally determined that a size 4 pool works
  // well for the coverage builder.
  // TODO(https://crbug.com/1059335): Check if this is an appropriate value for
  // the PGO builds.
  int pool_index = base::RandInt(0, 3);
  std::string filename = base::StrCat(
      {"child_pool-", base::NumberToString(pool_index), ".profraw"});
#if defined(OS_WIN)
  path = path.Append(base::UTF8ToWide(filename));
#else
  path = path.Append(filename);
#endif
  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                   base::File::FLAG_WRITE;

  base::File file(path, flags);
  if (!file.IsValid()) {
    LOG(ERROR) << "Opening file: " << path << " failed with "
               << file.error_details();
  }

  return file;
}
WaitForProcessesToDumpProfilingInfo::WaitForProcessesToDumpProfilingInfo() =
    default;
WaitForProcessesToDumpProfilingInfo::~WaitForProcessesToDumpProfilingInfo() =
    default;

void WaitForProcessesToDumpProfilingInfo::WaitForAll() {
  base::RunLoop nested_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  // Some of the waitable events will be signaled on the main thread, use a
  // nested run loop to ensure we're not preventing them from signaling.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(
          &WaitForProcessesToDumpProfilingInfo::WaitForAllOnThreadPool,
          base::Unretained(this), nested_run_loop.QuitClosure()));
  nested_run_loop.Run();
}

void WaitForProcessesToDumpProfilingInfo::WaitForAllOnThreadPool(
    base::OnceClosure quit_closure) {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_blocking;

  std::vector<base::WaitableEvent*> events_raw;
  events_raw.reserve(events_.size());
  for (const auto& iter : events_)
    events_raw.push_back(iter.get());

  // Wait for all the events to be signaled.
  while (events_raw.size()) {
    size_t index =
        base::WaitableEvent::WaitMany(events_raw.data(), events_raw.size());
    events_raw.erase(events_raw.begin() + index);
  }

  std::move(quit_closure).Run();
}

base::WaitableEvent*
WaitForProcessesToDumpProfilingInfo::GetNewWaitableEvent() {
  events_.push_back(std::make_unique<base::WaitableEvent>());
  return events_.back().get();
}

}  // namespace content
