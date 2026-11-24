// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include <algorithm>
#include <limits>
#include <optional>

#include "base/byte_size.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/ostream_operators.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"

namespace {

constexpr int kMaxOldFolders = 100;

// Returns a fully qualified name from path and name, using a given name prefix
// and index number. For instance, if the arguments are "/foo", "bar" and 5, it
// will return "/foo/old_bar_005".
base::FilePath GetPrefixedName(const base::FilePath& path,
                               const base::SafeBaseName& basename,
                               int index) {
  const std::string index_str = base::StringPrintf("_%03d", index);
  const base::FilePath::StringType filename =
      base::StrCat({FILE_PATH_LITERAL("old_"), basename.path().value(),
#if BUILDFLAG(IS_WIN)
                    base::ASCIIToWide(index_str)
#else
                    index_str
#endif
      });
  return path.Append(filename);
}

base::FilePath GetTempCacheName(const base::FilePath& dirname,
                                const base::SafeBaseName& basename) {
  // We'll attempt to have up to kMaxOldFolders folders for deletion.
  for (int i = 0; i < kMaxOldFolders; i++) {
    base::FilePath to_delete = GetPrefixedName(dirname, basename, i);
    if (!base::PathExists(to_delete)) {
      return to_delete;
    }
  }
  return base::FilePath();
}

void CleanupTemporaryDirectories(const base::FilePath& path) {
  const base::FilePath dirname = path.DirName();
  const std::optional<base::SafeBaseName> basename =
      base::SafeBaseName::Create(path);
  if (!basename.has_value()) {
    return;
  }
  for (int i = 0; i < kMaxOldFolders; i++) {
    base::FilePath to_delete = GetPrefixedName(dirname, *basename, i);
    disk_cache::DeleteCache(to_delete, /*remove_folder=*/true);
  }
}

bool MoveDirectoryToTemporaryDirectory(const base::FilePath& path) {
  const base::FilePath dirname = path.DirName();
  const std::optional<base::SafeBaseName> basename =
      base::SafeBaseName::Create(path);
  if (!basename.has_value()) {
    return false;
  }
  const base::FilePath destination = GetTempCacheName(dirname, *basename);
  if (destination.empty()) {
    return false;
  }
  return disk_cache::MoveCache(path, destination);
}

// In order to process a potentially large number of files, we'll rename the
// cache directory to old_ + original_name + number, (located on the same parent
// directory), and use a worker thread to delete all the files on all the stale
// cache directories. The whole process can still fail if we are not able to
// rename the cache directory (for instance due to a sharing violation), and in
// that case a cache for this profile (on the desired path) cannot be created.
bool CleanupDirectoryInternal(const base::FilePath& path) {
  const base::FilePath path_to_pass = path.StripTrailingSeparators();
  bool result = MoveDirectoryToTemporaryDirectory(path_to_pass);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CleanupTemporaryDirectories, path_to_pass));

  return result;
}

base::ByteSize PreferredCacheSizeInternal(base::ByteSize available) {
  using disk_cache::kDefaultCacheSize;
  // Return 80% of the available space if there is not enough space to use
  // kDefaultCacheSize.
  if (available < kDefaultCacheSize * 10 / 8) {
    return available * 8 / 10;
  }

  // Return kDefaultCacheSize if it uses 10% to 80% of the available space.
  if (available < kDefaultCacheSize * 10) {
    return kDefaultCacheSize;
  }

  // Return 10% of the available space if the target size
  // (2.5 * kDefaultCacheSize) is more than 10%.
  if (available < kDefaultCacheSize * 25) {
    return available / 10;
  }

  // Return the target size (2.5 * kDefaultCacheSize) if it uses 10% to 1%
  // of the available space.
  if (available < kDefaultCacheSize * 250) {
    return kDefaultCacheSize * 5 / 2;
  }

  // Return 1% of the available space.
  return available / 100;
}

}  // namespace

namespace disk_cache {

const base::ByteSize kDefaultCacheSize = base::MiB(80);

BASE_FEATURE(kChangeGeneratedCodeCacheSizeExperiment,
             "ChangeGeneratedCodeCacheSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

void DeleteCache(const base::FilePath& path, bool remove_folder) {
  if (remove_folder) {
    if (!base::DeletePathRecursively(path)) {
      LOG(WARNING) << "Unable to delete cache folder.";
    }
    return;
  }

  base::FileEnumerator iter(
      path,
      /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file = iter.Next(); !file.value().empty();
       file = iter.Next()) {
    if (!base::DeletePathRecursively(file)) {
      LOG(WARNING) << "Unable to delete cache.";
      return;
    }
  }
}

void CleanupDirectory(const base::FilePath& path,
                      base::OnceCallback<void(bool)> callback) {
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(CleanupDirectoryInternal, path),
      std::move(callback));
}

bool CleanupDirectorySync(const base::FilePath& path) {
  base::ScopedAllowBlocking allow_blocking;

  return CleanupDirectoryInternal(path);
}

// Returns the preferred maximum number of bytes for the cache given the
// number of available bytes.
base::ByteSize PreferredCacheSize(std::optional<base::ByteSize> available,
                                  net::CacheType type) {
  // Percent of cache size to use, relative to the default size. "100" means to
  // use 100% of the default size.
  int percent_relative_size = 100;

// See go/change-disk-cache-size-results-2024 for an explanation of why the
// size varies by platform.
#if !BUILDFLAG(IS_WIN)
  if (type == net::DISK_CACHE) {
    percent_relative_size = 400;
  }
#endif

  if (base::FeatureList::IsEnabled(
          disk_cache::kChangeGeneratedCodeCacheSizeExperiment) &&
      type == net::GENERATED_BYTE_CODE_CACHE) {
    percent_relative_size = base::GetFieldTrialParamByFeatureAsInt(
        disk_cache::kChangeGeneratedCodeCacheSizeExperiment,
        "percent_relative_size", /*default_value=*/400);
  }

  // Clamp scaling, as a safety check, to avoid overflow.
  percent_relative_size = std::clamp(percent_relative_size, 100, 400);

  base::ClampedNumeric<int64_t> scaled_default_disk_cache_size =
      (base::ClampedNumeric<int64_t>(disk_cache::kDefaultCacheSize.InBytes()) *
       percent_relative_size) /
      100;

  base::ClampedNumeric<int64_t> preferred_cache_size =
      scaled_default_disk_cache_size;

  // If available disk space is known, use it to compute a better value for
  // preferred_cache_size.
  if (available) {
    preferred_cache_size =
        PreferredCacheSizeInternal(available.value()).InBytes();

    // If the preferred cache size is less than 20% of the available space,
    // scale for the field trial, capping the scaled value at 20% of the
    // available space.
    if (preferred_cache_size < (available.value() / 5).InBytes()) {
      const base::ClampedNumeric<int64_t> clamped_available(
          available->InBytes());
      preferred_cache_size =
          std::min((preferred_cache_size * percent_relative_size) / 100,
                   clamped_available / 5);
    }
  }

  // Limit cache size to somewhat less than INT32_MAX to avoid potential
  // integer overflows in cache backend implementations.
  //
  // Note: the 4x limit is of course far below that; historically it came
  // from the blockfile backend with the following explanation:
  // "Let's not use more than the default size while we tune-up the performance
  // of bigger caches."
  base::ClampedNumeric<int64_t> size_limit = scaled_default_disk_cache_size * 4;
  // Native code entries can be large, so we would like a larger cache.
  // Make the size limit 50% larger in that case.
  if (type == net::GENERATED_NATIVE_CODE_CACHE) {
    size_limit = (size_limit / 2) * 3;
  } else if (type == net::GENERATED_WEBUI_BYTE_CODE_CACHE) {
    size_limit = std::min(
        size_limit, base::ClampedNumeric<int64_t>(kMaxWebUICodeCacheSize));
  }

  DCHECK_LT(size_limit, std::numeric_limits<int32_t>::max());
  return base::ByteSize(
      static_cast<uint32_t>(std::min(preferred_cache_size, size_limit)));
}

base::ByteSize PreferredCacheSizeForPath(const base::FilePath& path,
                                         net::CacheType type) {
  std::optional<base::SysInfo::DiskSpaceInfo> disk_space =
      base::SysInfo::AmountOfDiskSpace(path);

  return PreferredCacheSize(
      disk_space ? std::make_optional(disk_space->available) : std::nullopt,
      type);
}

}  // namespace disk_cache
