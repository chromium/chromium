// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_file_enumerator.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"

// We have an optimized implementation for POSIX, and a fallback
// implementation for other platforms.

namespace disk_cache {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

SimpleFileEnumerator::SimpleFileEnumerator(const base::FilePath& path)
    : path_(path), dir_(opendir(path.value().c_str())), has_error_(!dir_) {
  if (has_error_) {
    PLOG(ERROR) << "opendir " << path;
  }
}
SimpleFileEnumerator::~SimpleFileEnumerator() = default;

bool SimpleFileEnumerator::HasError() const {
  return has_error_;
}

std::optional<SimpleFileEnumerator::Entry> SimpleFileEnumerator::Next() {
  if (!dir_) {
    return std::nullopt;
  }
  while (true) {
    // errno must be set to 0 before every readdir() call to detect errors.
    errno = 0;
    dirent* entry = readdir(dir_.get());
    if (!entry) {
      // Some implementations of readdir() (particularly older versions of
      // Android Bionic) may leave errno set to EINTR even after they handle
      // this case internally. It's safe to ignore EINTR in that case.
      if (errno && errno != EINTR) {
        PLOG(ERROR) << "readdir " << path_;
        has_error_ = true;
        dir_ = nullptr;
        return std::nullopt;
      }
      break;
    }

    const std::string filename(entry->d_name);
    if (filename == "." || filename == "..") {
      continue;
    }
    base::FilePath path = path_.Append(base::FilePath(filename));
    base::File::Info file_info;
    if (!base::GetFileInfo(path, &file_info)) {
      LOG(ERROR) << "Could not get file info for " << path;
      continue;
    }
    if (file_info.is_directory) {
      continue;
    }
    return std::make_optional<Entry>(std::move(path), file_info.size,
                                     file_info.last_accessed,
                                     file_info.last_modified);
  }
  dir_ = nullptr;
  return std::nullopt;
}

#else
SimpleFileEnumerator::SimpleFileEnumerator(const base::FilePath& path)
    : enumerator_(path,
                  /*recursive=*/false,
                  base::FileEnumerator::FILES) {}
SimpleFileEnumerator::~SimpleFileEnumerator() = default;

bool SimpleFileEnumerator::HasError() const {
  return enumerator_.GetError() != base::File::FILE_OK;
}

std::optional<SimpleFileEnumerator::Entry> SimpleFileEnumerator::Next() {
  base::FilePath path = enumerator_.Next();
  if (path.empty()) {
    return std::nullopt;
  }
  base::FileEnumerator::FileInfo info = enumerator_.GetInfo();
  return std::make_optional<Entry>(std::move(path), info.GetSize(),
                                   /*last_accessed=*/base::Time(),
                                   info.GetLastModifiedTime());
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

}  // namespace disk_cache
