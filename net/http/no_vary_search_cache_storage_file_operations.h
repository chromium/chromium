// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_NO_VARY_SEARCH_CACHE_STORAGE_FILE_OPERATIONS_H_
#define NET_HTTP_NO_VARY_SEARCH_CACHE_STORAGE_FILE_OPERATIONS_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/base/net_export.h"

namespace base {
class FilePath;
}

namespace net {

// An interface for the file operations needed by NoVarySearchCacheStorage.
// This is intended for use on a background thread; all operations are
// blocking. The main purpose of this interface is to simplify testing by
// separating the file-handling logic from the rest. All files are referenced
// by filenamew which must be ASCII, not include path separators, and not be
// "." or "..". In official builds there will only be one implementation of
// these interfaces, so calls will be devirtualized and may be inlined.
class NET_EXPORT NoVarySearchCacheStorageFileOperations {
 public:
  // TODO(https://crbug.com/433551601): Remove this once the rate of migrations
  // drops to zero.
  static constexpr char kLegacyNoVarySearchDirName[] = "no-vary-search";

  // Result of a call to Load().
  struct NET_EXPORT LoadResult {
    std::vector<uint8_t> contents;
    base::Time last_modified;

    // Required by chromium style plugin.
    LoadResult();
    LoadResult(const LoadResult&);
    LoadResult(LoadResult&&);
    LoadResult& operator=(const LoadResult&);
    LoadResult& operator=(LoadResult&&);
    ~LoadResult();
  };

  // A simple writer interface for appending to a file.
  class NET_EXPORT Writer {
   public:
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    // Cleanly closes the file.
    virtual ~Writer();

    // Appends `data` to the file. On successful completion, `data` has been
    // completely written to the file (but it is not guaranteed to have been
    // written to the underlying storage). Writes all data at once to be as
    // close to atomic as the underlying system will allow. Returns false on
    // failure.
    virtual bool Write(base::span<const uint8_t> data) = 0;

   protected:
    Writer() = default;
  };

  // Creates a NoVarySearchCacheStorageFileOperations object that accesses the
  // real file system. All filenames will be treated as relative to
  // `dedicated_path`. If there are existing persisted files inside
  // `legacy_path` they will be moved to `dedicated_path` during the call to
  // Init().
  // TODO(https://crbug.com/433551601): Remove `legacy_path` once the rate of
  // migrations drops to zero.
  static std::unique_ptr<NoVarySearchCacheStorageFileOperations> Create(
      const base::FilePath& dedicated_path,
      const base::FilePath& legacy_path);

  NoVarySearchCacheStorageFileOperations(
      const NoVarySearchCacheStorageFileOperations&) = delete;
  NoVarySearchCacheStorageFileOperations& operator=(
      const NoVarySearchCacheStorageFileOperations&) = delete;

  virtual ~NoVarySearchCacheStorageFileOperations();

  // Performs any cleanup or initialization operations that need to be done
  // before using the object. Must be called exactly once after `path` is ready
  // to be accessed but before calling any of the other methods. Returns true if
  // the subdirectory `kNoVarySearchDirName` probably exists when Init()
  // returns.
  virtual bool Init() = 0;

  // Loads the complete contents of the file `filename` into memory and
  // returns it and its last modified time. Returns the appropriate Error on
  // failure. If the file is larger than `max_size`, returns
  // base::File::FILE_ERROR_NO_MEMORY. If the file is modified during loading,
  // the result is not guaranteed to be internally consistent.
  virtual base::expected<LoadResult, base::File::Error> Load(
      std::string_view filename,
      size_t max_size) = 0;

  // Writes every segment in `segments` to `filename` in sequence.
  // Segments are permitted to be empty. `filename` will not be
  // overwritten or truncated on error. This is guaranteed by writing to a
  // temporary file first and then renaming it over `filename` once writes
  // are complete. For efficiency, AtomicSave() does not attempt to
  // synchronize writes to the underlying storage, so data can still be
  // lost in the event of OS crash or power loss.
  virtual base::expected<void, base::File::Error> AtomicSave(
      std::string_view filename,
      base::span<const base::span<const uint8_t>> segments) = 0;

  // Opens `filename` for writing. If it exists, it is deleted first, so the
  // file is always empty on successful return from this method. Destroying
  // the returned Writer object cleanly closes the file.
  virtual base::expected<std::unique_ptr<Writer>, base::File::Error>
  CreateWriter(std::string_view filename) = 0;

 protected:
  NoVarySearchCacheStorageFileOperations() = default;
};

}  // namespace net

#endif  // NET_HTTP_NO_VARY_SEARCH_CACHE_STORAGE_FILE_OPERATIONS_H_
