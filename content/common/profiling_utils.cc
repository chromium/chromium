// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/profiling_utils.h"

#include <limits>
#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

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
#if BUILDFLAG(IS_ANDROID)
  base::PathService::Get(base::DIR_TEMP, &path);
  path = path.Append("pgo_profiles/");
  // Lacros is similar to Android that it's running on a device that is not
  // the host machine and environment variables aren't well supported.
  // But Lacros also need to pass in the path so it is the same path as
  // isolate test output folder on bots.
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  path = base::CommandLine::ForCurrentProcess()
             ->GetSwitchValuePath(switches::kLLVMProfileFile)
             .DirName();
#else
  std::string prof_template;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->GetVar("LLVM_PROFILE_FILE", &prof_template)) {
#if BUILDFLAG(IS_WIN)
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
  // TODO(crbug.com/40121559): Check if this is an appropriate value for
  // the PGO builds.
  int pool_index = base::RandInt(0, 3);
  std::string filename = base::StrCat(
      {"child_pool-", base::NumberToString(pool_index), ".profraw"});
#if BUILDFLAG(IS_WIN)
  path = path.Append(base::UTF8ToWide(filename));
#else
  path = path.Append(filename);
#endif
  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                   base::File::FLAG_WRITE;

  // The profiling file is passed to an untrusted process.
  flags = base::File::AddFlagsForPassingToUntrustedProcess(flags);

  base::File file(path, flags);
  if (!file.IsValid()) {
    LOG(ERROR) << "Opening file: " << path << " failed with "
               << file.error_details();
  }

  return file;
}

}  // namespace content
