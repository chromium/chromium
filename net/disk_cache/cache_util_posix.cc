// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <string_view>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace disk_cache {

namespace {

struct CloseDir {
  void operator()(DIR* const dir) const {
    if (dir) {
      IGNORE_EINTR(closedir(dir));
    }
  }
};

bool DeleteDirectoryContents(int dir_fd);

bool DeleteEntryAt(int at_fd, const char* name) {
  struct stat st;
  if (HANDLE_EINTR(fstatat(at_fd, name, &st, AT_SYMLINK_NOFOLLOW)) < 0) {
    return errno == ENOENT;
  }

  if (!S_ISDIR(st.st_mode)) {
    return unlinkat(at_fd, name, 0) == 0 || errno == ENOENT;
  }

  const int child_fd = HANDLE_EINTR(
      openat(at_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  if (child_fd < 0) {
    return errno == ENOENT;
  }

  if (!DeleteDirectoryContents(child_fd)) {
    return false;
  }

  return unlinkat(at_fd, name, AT_REMOVEDIR) == 0 || errno == ENOENT;
}

// Takes ownership of `dir_fd` and deletes all entries inside it without
// removing the directory itself or following symlinks.
bool DeleteDirectoryContents(int dir_fd) {
  const std::unique_ptr<DIR, CloseDir> dir(fdopendir(dir_fd));
  if (!dir) {
    IGNORE_EINTR(close(dir_fd));
    return false;
  }

  while (true) {
    errno = 0;
    const dirent* const entry = readdir(dir.get());
    if (!entry) {
      break;
    }

    const std::string_view name(entry->d_name);
    if (name == "." || name == "..") {
      continue;
    }

    if (!DeleteEntryAt(dir_fd, entry->d_name)) {
      return false;
    }
  }

  return errno == 0;
}

}  // namespace

namespace internal {

bool DeleteCacheContents(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const int dir_fd = HANDLE_EINTR(open(
      path.value().c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  if (dir_fd < 0) {
    return errno == ENOENT;
  }
  return DeleteDirectoryContents(dir_fd);
}

}  // namespace internal

bool MoveCache(const base::FilePath& from_path, const base::FilePath& to_path) {
  if (from_path.empty() || to_path.empty() || from_path.ReferencesParent() ||
      to_path.ReferencesParent()) {
    return false;
  }
  const base::FilePath stripped_from = from_path.StripTrailingSeparators();
  const base::FilePath stripped_to = to_path.StripTrailingSeparators();
  if (base::IsLink(stripped_from) || !base::DirectoryExists(stripped_from) ||
      base::IsLink(stripped_to) || base::PathExists(stripped_to)) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS)
  // For ChromeOS, we don't actually want to rename the cache
  // directory, because if we do, then it'll get recreated through the
  // encrypted filesystem (with encrypted names), and we won't be able
  // to see these directories anymore in an unmounted encrypted
  // filesystem, so we just move each item in the cache to a new
  // directory.
  if (!base::CreateDirectory(stripped_to) || base::IsLink(stripped_to)) {
    LOG(ERROR) << "Unable to create destination cache directory.";
    return false;
  }
  base::FileEnumerator iter(stripped_from, /*recursive=*/false,
                            base::FileEnumerator::DIRECTORIES |
                                base::FileEnumerator::FILES |
                                base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath name = iter.Next(); !name.value().empty();
       name = iter.Next()) {
    if (base::IsLink(name) || base::IsLink(stripped_from) ||
        base::IsLink(stripped_to)) {
      LOG(ERROR) << "Unable to move cache item.";
      return false;
    }
    base::FilePath destination = stripped_to.Append(name.BaseName());
    if (!base::ReplaceFile(name, destination, /*error=*/nullptr)) {
      LOG(ERROR) << "Unable to move cache item.";
      return false;
    }
  }
  return true;
#else
  // The destination is on the same filesystem, so use ReplaceFile (rename)
  // directly rather than base::Move's copy-and-delete fallback.
  return base::ReplaceFile(stripped_from, stripped_to, /*error=*/nullptr);
#endif
}

}  // namespace disk_cache
