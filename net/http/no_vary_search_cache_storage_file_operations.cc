// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache_storage_file_operations.h"

#include <stdint.h>

#include "net/http/no_vary_search_cache_storage.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>  // For {Get,Set}FileAttributes
#endif                // BUILDFLAG(IS_WIN)

#include <algorithm>
#include <type_traits>
#include <utility>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/pickle.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

#if BUILDFLAG(IS_WIN)
#include "base/threading/platform_thread.h"  // for PlatformThread::Sleep()
#endif                                       // BUILDFLAG(IS_WIN)

namespace net {

namespace {

// NoVarySearchCacheStorageFileOperations is a very long name.
using FileOperations = NoVarySearchCacheStorageFileOperations;

using enum base::File::Error;

// Implementation of FileOperations::Writer that appends to a real file.
class RealWriter final : public FileOperations::Writer {
 public:
  explicit RealWriter(base::File file) : file_(std::move(file)) {}

  ~RealWriter() override { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  bool Write(base::span<const uint8_t> data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return file_.WriteAtCurrentPosAndCheck(data);
  }

 private:
  base::File file_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// True if `filename` should be accepted by FileOperations methods.
bool IsAcceptableFilename(std::string_view filename) {
  return base::IsStringASCII(filename) &&
         std::ranges::none_of(filename, base::FilePath::IsSeparator) &&
         filename != "." && filename != "..";
}

// Logs `error` to histogram `name`.
void UmaHistogramFileError(std::string_view name, base::File::Error error) {
  base::UmaHistogramExactLinear(name, -error, -FILE_ERROR_MAX);
}

// Creates the directory `path` and all non-existent parent directories if
// possible. Reports the results to histograms using `histogram_suffix`.
bool CreateDirectoryIfNotExists(const base::FilePath& path,
                                std::string_view histogram_suffix) {
  // The result of trying to create the directory.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(CreateDirectoryResult)
  enum class CreateDirectoryResult {
    kAlreadyExisted = 0,
    kCreated = 1,
    kCreateFailed = 2,
    kMaxValue = kCreateFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:NoVarySearchDirectoryCreateResult)

  const CreateDirectoryResult result = [&] {
    if (base::DirectoryExists(path)) {
      return CreateDirectoryResult::kAlreadyExisted;
    }

    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(path, &error)) {
      UmaHistogramFileError(
          base::StrCat({"HttpCache.NoVarySearch.DirectoryCreateError.",
                        histogram_suffix}),
          error);
      return CreateDirectoryResult::kCreateFailed;
    }

    return CreateDirectoryResult::kCreated;
  }();

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"HttpCache.NoVarySearch.DirectoryCreateResult.", histogram_suffix}),
      result);

  return result != CreateDirectoryResult::kCreateFailed;
}

// Deletes `path`. Returns true on success. Logs the error code to the histogram
// named by concatenating `histogram_name_parts` and returns false if deletion
// fails.
bool DeleteLoggingErrors(
    const base::FilePath& path,
    base::span<const std::string_view> histogram_name_parts) {
  if (base::DeleteFile(path)) {
    return true;
  }

  UmaHistogramFileError(base::StrCat(histogram_name_parts),
                        base::File::GetLastFileError());

  return false;
}

// Renames `old_path` to `new_path` if `old_path` exists and `new_path` does
// not. If both exist, deletes `old_path`. Records results to histograms using
// `histogram_suffix`.
void RenameOrDeleteIfExists(const base::FilePath& old_path,
                            const base::FilePath& new_path,
                            std::string_view histogram_suffix) {
  // The result of the attempted rename or delete operation.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(RenameResult)
  enum class RenameResult {
    kSourceDidNotExist = 0,
    kSourceDeleted = 1,
    kDeletionFailed = 2,
    kRenamed = 3,
    kRenameFailed = 4,
    kMaxValue = kRenameFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:NoVarySearchRenameOrDeleteResult)

  const RenameResult result = [&] {
    if (!base::PathExists(old_path)) {
      return RenameResult::kSourceDidNotExist;
    }

    if (base::PathExists(new_path)) {
      return DeleteLoggingErrors(
                 old_path,
                 {"HttpCache.NoVarySearch.InitDeleteError.", histogram_suffix})
                 ? RenameResult::kSourceDeleted
                 : RenameResult::kDeletionFailed;
    }

    base::File::Error error;
    if (!base::ReplaceFile(old_path, new_path, &error)) {
      // We don't attempt retries on Windows. If something has the file open we
      // just give up. This rename functionality is purely best-effort and it's
      // not critical if it fails, as the NoVarySearchCache will just be
      // recreated.
      UmaHistogramFileError(
          base::StrCat(
              {"HttpCache.NoVarySearch.InitRenameError.", histogram_suffix}),
          error);
      return RenameResult::kRenameFailed;
    }

    return RenameResult::kRenamed;
  }();

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"HttpCache.NoVarySearch.RenameOrDeleteResult.", histogram_suffix}),
      result);
}

void DeleteIfExists(const base::FilePath& path,
                    std::string_view histogram_suffix) {
  // base::DeleteFile actually already tests if the file exists, but since it
  // almost always won't we can save some time by doing it ourselves.
  if (!base::PathExists(path)) {
    return;
  }

  DeleteLoggingErrors(
      path, {"HttpCache.NoVarySearch.DeleteIfExistsError.", histogram_suffix});
}

constexpr std::string_view kSnapshotFilename =
    NoVarySearchCacheStorage::kSnapshotFilename;

void DeleteTempFileIfNeeded(const base::FilePath& path,
                            std::string_view histogram_suffix) {
  const std::string snapshot_tempfile =
      base::StrCat({kSnapshotFilename, "-new"});
  DeleteIfExists(path.AppendASCII(snapshot_tempfile), histogram_suffix);
}

void MoveOldFilesIfNeededBetween(const base::FilePath& old_path,
                                 const base::FilePath& new_path,
                                 std::string_view old_path_histogram_suffix) {
  static constexpr std::string_view kJournalFilename =
      NoVarySearchCacheStorage::kJournalFilename;

  RenameOrDeleteIfExists(
      old_path.AppendASCII(kSnapshotFilename),
      new_path.AppendASCII(kSnapshotFilename),
      base::StrCat({old_path_histogram_suffix, ".Snapshot"}));
  RenameOrDeleteIfExists(old_path.AppendASCII(kJournalFilename),
                         new_path.AppendASCII(kJournalFilename),
                         base::StrCat({old_path_histogram_suffix, ".Journal"}));
  DeleteTempFileIfNeeded(old_path, old_path_histogram_suffix);
}

void MoveOldFilesIfNeeded(const base::FilePath& legacy_path,
                          const base::FilePath& legacy_subdirectory,
                          const base::FilePath& path) {
  if (base::DirectoryExists(legacy_subdirectory)) {
    // We should do a two-step move to ensure nothing is left behind.
    MoveOldFilesIfNeededBetween(legacy_path, legacy_subdirectory, "Parent");
    MoveOldFilesIfNeededBetween(legacy_subdirectory, path, "NoVarySearch");
    if (!base::DeleteFile(legacy_subdirectory)) {
      UmaHistogramFileError(
          "HttpCache.NoVarySearch.LegacySubdirectoryDeleteError",
          base::File::GetLastFileError());
    }
  } else {
    // A one-step move is sufficient.
    MoveOldFilesIfNeededBetween(legacy_path, path, "Parent");
  }
}

#if BUILDFLAG(IS_WIN)
// Attempt to replace `destination` with `source`, retrying on failure. Only
// needed on Windows, because only on Windows do virus checkers and other
// software open files preventing you from renaming them. Based on code from
// //base/files/important_file_writer.cc. Function signature must match
// base::ReplaceFile().
bool ReplaceFileWithRetries(const base::FilePath& source,
                            const base::FilePath& destination,
                            base::File::Error* error) {
  // These settings are more aggressive than used by ImportantFileWriter.
  static constexpr int kReplaceRetries = 50;
  static constexpr base::TimeDelta kReplacePauseInterval =
      base::Milliseconds(10);

  // Unlike ImportantFileWriter, we don't try to boost priority to win the race
  // against virus checkers and other interfering software, instead just relying
  // on being persistent.
  int try_count = 0;
  bool result = false;
  base::File::Error last_error = base::File::FILE_OK;
  for (; !result && try_count < kReplaceRetries; ++try_count) {
    result = base::ReplaceFile(source, destination, &last_error);
    if (result) {
      break;
    }
    if (last_error == base::File::FILE_ERROR_ACCESS_DENIED) {
      // Attempt to fix permission problems. Avoid doing this by
      // default because it's not actually atomic.
      DWORD attrs = ::GetFileAttributes(destination.value().c_str());
      if (attrs != INVALID_FILE_ATTRIBUTES) {
        ::SetFileAttributes(destination.value().c_str(),
                            attrs & ~FILE_ATTRIBUTE_READONLY);
      }
    } else if (last_error != base::File::FILE_ERROR_IN_USE) {
      // We don't expect to recover from this error by retry, so just give up.
      break;
    }
    base::PlatformThread::Sleep(kReplacePauseInterval);
  }
  if (result) {
    base::UmaHistogramExactLinear("HttpCache.NoVarySearch.ReplaceFileTryCount",
                                  try_count, kReplaceRetries);
  } else {
    *error = last_error;
  }
  return result;
}
#endif  // BUILDFLAG(IS_WIN)

// Implementation of FileOperations that operates on real files.
class RealFileOperations : public FileOperations {
 public:
  using enum base::File::Flags;

  explicit RealFileOperations(const base::FilePath& dedicated_path,
                              const base::FilePath& legacy_path)
      : legacy_path_(legacy_path), path_(dedicated_path) {
    // It's normal to construct this on a different thread than it will be used.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~RealFileOperations() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  bool Init() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const base::FilePath legacy_subdirectory =
        legacy_path_.AppendASCII(kLegacyNoVarySearchDirName);

    if (!CreateDirectoryIfNotExists(path_,
                                    /*histogram_suffix=*/"Dedicated")) {
      return false;
    }

    if (base::DirectoryExists(legacy_path_)) {
      // TODO(https://crbug.com/421927600): Remove this in December 2025
      // provided the kSourceDidNotExist bucket of the
      // HttpCache.NoVarySearch.RenameOrDeleteResult.Snapshot histogram has
      // reached 100%.
      MoveOldFilesIfNeeded(legacy_path_, legacy_subdirectory, path_);
    }

    DeleteTempFileIfNeeded(path_, "Dedicated");

    return true;
  }

  base::expected<LoadResult, base::File::Error> Load(std::string_view filename,
                                                     size_t max_size) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    const base::FilePath path = GetPath(filename);
    if (path.empty()) {
      return base::unexpected(FILE_ERROR_SECURITY);
    }

    base::File file(path, FLAG_OPEN | FLAG_READ);
    if (!file.IsValid()) {
      return base::unexpected(file.error_details());
    }

    base::File::Info info;
    if (!file.GetInfo(&info)) {
      return base::unexpected(FILE_ERROR_FAILED);
    }

    CHECK_GE(info.size, 0);
    if (base::StrictNumeric(info.size) > max_size) {
      return base::unexpected(FILE_ERROR_NO_MEMORY);
    }

    // This cast is safe because we checked that 0 <= info.size <= max_size, and
    // max_size is a size_t.
    const size_t size = static_cast<size_t>(info.size);

    LoadResult result;

    result.contents.resize(size);
    result.last_modified = info.last_modified;

    std::optional<size_t> maybe_bytes = file.ReadAtCurrentPos(result.contents);
    if (!maybe_bytes) {
      return base::unexpected(FILE_ERROR_IO);
    }
    size_t read_bytes = maybe_bytes.value();
    CHECK_LE(read_bytes, size);
    if (read_bytes < size) {
      // The file shrank.
      result.contents.resize(read_bytes);
    }

    return result;
  }

  base::expected<void, base::File::Error> AtomicSave(
      std::string_view filename,
      base::span<const base::span<const uint8_t>> segments) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::FilePath path = GetPath(filename);
    if (path.empty()) {
      return base::unexpected(FILE_ERROR_SECURITY);
    }

    // Use a consistent temporary file name so that it will eventually be
    // cleaned up on a future run if we crash.
    base::FilePath temp_path = path.InsertBeforeExtensionASCII("-new");

    // To defend against permission problems, delete `temp_path` if it already
    // exists. It doesn't matter if this fails.
    base::DeleteFile(temp_path);

    base::File temp_file(temp_path, FLAG_CREATE_ALWAYS | FLAG_WRITE);

    if (!temp_file.IsValid()) {
      return base::unexpected(temp_file.error_details());
    }

    for (auto segment : segments) {
      if (segment.empty()) {
        continue;
      }

      if (!temp_file.WriteAtCurrentPosAndCheck(segment)) {
        return base::unexpected(FILE_ERROR_IO);
      }
    }

    temp_file.Close();

    auto replace_file_func = base::ReplaceFile;

#if BUILDFLAG(IS_WIN)
    replace_file_func = ReplaceFileWithRetries;
#endif

    base::File::Error replace_error = FILE_OK;

    if (!replace_file_func(temp_path, path, &replace_error)) {
      UmaHistogramFileError("HttpCache.NoVarySearch.ReplaceFileError",
                            replace_error);
      return base::unexpected(replace_error);
    }

    return base::ok();
  }

  base::expected<std::unique_ptr<Writer>, base::File::Error> CreateWriter(
      std::string_view filename) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::FilePath path = GetPath(filename);
    if (path.empty()) {
      return base::unexpected(FILE_ERROR_SECURITY);
    }

    // To defend against permission problems, delete `path` if it already
    // exists. Ignore errors.
    base::DeleteFile(path);

    base::File file(path, FLAG_CREATE_ALWAYS | FLAG_WRITE);

    if (!file.IsValid()) {
      return base::unexpected(file.error_details());
    }

    return std::make_unique<RealWriter>(std::move(file));
  }

 private:
  base::FilePath GetPath(std::string_view filename)
      VALID_CONTEXT_REQUIRED(sequence_checker_) {
    if (!IsAcceptableFilename(filename)) {
      return base::FilePath();
    }

    return path_.AppendASCII(filename);
  }

  // TODO(https://crbug.com/433551601): Remove `legacy_path_` once the
  // SourceDidNotExist bucket of all the
  // HttpCache.NoVarySearch.RenameOrDeleteResult.{NoVarySearch,Parent}.{Journal,Snapshot}
  // histograms has reached 100.00%.
  const base::FilePath legacy_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  const base::FilePath path_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

FileOperations::LoadResult::LoadResult() = default;
FileOperations::LoadResult::LoadResult(const LoadResult&) = default;
FileOperations::LoadResult::LoadResult(LoadResult&&) = default;

FileOperations::LoadResult& FileOperations::LoadResult::operator=(
    const LoadResult&) = default;

FileOperations::LoadResult& FileOperations::LoadResult::operator=(
    LoadResult&&) = default;

FileOperations::LoadResult::~LoadResult() = default;

FileOperations::Writer::~Writer() = default;

NoVarySearchCacheStorageFileOperations::
    ~NoVarySearchCacheStorageFileOperations() = default;

std::unique_ptr<FileOperations> FileOperations::Create(
    const base::FilePath& dedicated_path,
    const base::FilePath& legacy_path) {
  return std::make_unique<RealFileOperations>(dedicated_path, legacy_path);
}

}  // namespace net
