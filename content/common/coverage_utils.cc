// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/coverage_utils.h"

#include <memory>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

namespace content {

base::File OpenCoverageFile() {
  base::ScopedAllowBlockingForTesting allows_blocking;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string prof_template;
  base::FilePath path;
  if (env->GetVar("LLVM_PROFILE_FILE", &prof_template)) {
#if defined(OS_WIN)
    path = base::FilePath(base::UTF8ToUTF16(prof_template)).DirName();
#else
    path = base::FilePath(prof_template).DirName();
#endif
    base::CreateDirectory(path);
  } else {
    base::PathService::Get(base::DIR_CURRENT, &path);
  }

  // sajjadm@ and liaoyuke@ experimentally determined that a size 4 pool works
  // well for the coverage builder.
  int pool_index = base::RandInt(0, 3);
  std::string filename = base::StrCat(
      {"child_pool-", base::NumberToString(pool_index), ".profraw"});
#if defined(OS_WIN)
  path = path.Append(base::UTF8ToUTF16(filename));
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

}  // namespace content
