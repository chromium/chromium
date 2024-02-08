// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_FILE_ENUMERATOR_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_FILE_ENUMERATOR_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/disk_cache/disk_cache.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <dirent.h>
#include <sys/types.h>
#else
#include "base/files/file_enumerator.h"
#endif

namespace disk_cache {

// This is similar to base::SimpleFileEnumerator, but the implementation is
// optimized for the big directory use-case on POSIX. See
// https://crbug.com/270762 and https://codereview.chromium.org/22927018.
class NET_EXPORT SimpleFileEnumerator final {
 public:
  using Entry = BackendFileOperations::FileEnumerationEntry;

  explicit SimpleFileEnumerator(const base::FilePath& root_path);
  ~SimpleFileEnumerator();

  // Returns true if we've found an error during enumeration.
  bool HasError() const;

  // Returns the next item, or nullopt if there are no more results (including
  // the error case).
  std::optional<Entry> Next();

 private:
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  struct DirCloser {
    void operator()(DIR* dir) { closedir(dir); }
  };
  const base::FilePath path_;
  std::unique_ptr<DIR, DirCloser> dir_;
  bool has_error_ = false;
#else
  base::FileEnumerator enumerator_;
#endif
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_FILE_ENUMERATOR_H_
