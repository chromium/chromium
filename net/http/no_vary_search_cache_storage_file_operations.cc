// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache_storage_file_operations.h"

#include <stdint.h>

#if BUILDFLAG(IS_WIN)
#include <windows.h>  // For {Get,Set}FileAttributes
#endif                // BUILDFLAG(IS_WIN)

#include <algorithm>
#include <type_traits>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/pickle.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

#if BUILDFLAG(IS_WIN)
#include "base/threading/platform_thread.h"  // for PlatformThread::Sleep()
#endif                                       // BUILDFLAG(IS_WIN)

namespace net {

namespace {

// NoVarySearchCacheStorageFileOperations is a very long name.
using FileOperations = NoVarySearchCacheStorageFileOperations;

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
  using enum base::File::Error;

  explicit RealFileOperations(const base::FilePath& path) : path_(path) {
    // It's normal to construct this on a different thread than it will be used.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~RealFileOperations() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      base::UmaHistogramExactLinear("HttpCache.NoVarySearch.ReplaceFileError",
                                    -replace_error, -FILE_ERROR_MAX);
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

  base::FilePath path_ GUARDED_BY_CONTEXT(sequence_checker_);

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
    const base::FilePath& path) {
  return std::make_unique<RealFileOperations>(path);
}

}  // namespace net
