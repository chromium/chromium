// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_ANDROID_H_
#define CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_ANDROID_H_

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/font_unique_name_lookup/font_unique_name_lookup.mojom.h"

static_assert(BUILDFLAG(IS_ANDROID), "This implementation only works safely "
              "on Android due to the way it assumes font files to be "
              "read-only and unmodifiable.");

namespace content {

// Scans a set of font files for the full font name and postscript name
// information in the name table and builds a Protobuf lookup structure from
// it. The protobuf can be persisted to disk to the Android cache directory, and
// it can be read from disk as well. Provides the lookup structure as a
// ReadOnlySharedMemoryRegion. Performing lookup on it is done through
// FontTableMatcher.
class CONTENT_EXPORT FontUniqueNameLookup {
 public:
  FontUniqueNameLookup() = delete;

  // Retrieve an instance of FontUniqueNameLookup. On the first call to
  // GetInstance() this that will start a task reading the lookup table from
  // cache if there was a cached one, updating the lookup table if needed
  // (i.e. if there was an Android firmware update or no cached one existed)
  // from the standard Android font directories, and writing the updated lookup
  // table back to file.
  static FontUniqueNameLookup& GetInstance();

  // Construct a FontUniqueNameLookup given a cache directory path
  // |cache_directory| to persist the internal lookup table, a
  // FontFilesCollector to enumerate font files and a BuildFingerprintProvider
  // to access the Android build fingerprint.
  FontUniqueNameLookup(const base::FilePath& cache_directory);
  ~FontUniqueNameLookup();

  // Return a ReadOnlySharedMemoryRegion to access the serialized form of the
  // current lookup table. To be used with FontTableMatcher.
  base::ReadOnlySharedMemoryRegion DuplicateMemoryRegion();

  void QueueShareMemoryRegionWhenReady(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      blink::mojom::FontUniqueNameLookup::GetUniqueNameLookupTableCallback
          callback);

  // Returns true if an up-to-date, consistent font table is present.
  bool IsValid();

  // If an Android firmware update was detected by checking
  // BuildFingerprintProvider, call UpdateTable(). Do not use this method.
  // Instead, call GetInstance() to get an initialized instance. Publicly
  // exposed for testing.
  bool UpdateTableIfNeeded();
  // Rescan the files returned by the FontFilesCollector and rebuild the lookup
  // table by indexing them. Do not use this method. Instead, call GetInstance()
  // to get an initialized instance. Returns true if instance is valid after
  // updating, returns false if an error occured in acquiring memory or
  // serializing the scanned files to the shared memory region. Publicly exposed
  // for testing.
  bool UpdateTable();
  // Try to find a serialized lookup table in the directory specified at
  // construction and load it into memory. Do not use this method. Instead, call
  // GetInstance() to get an initialized instance. Publicly exposed for testing.
  bool LoadFromFile();
  // Serialize the current lookup table into a file in the the cache directory
  // specified at construction time. If an up to date table is present and
  // persisting fails, discard the internal table, as it might be that we were
  // not able to update the file the previous time. Do not use this
  // method. Instead, call GetInstance() to get an initialized
  // instance. Publicly exposed for testing.
  bool PersistToFile();

  // Override the internal font files enumeration with an explicit set of fonts
  // to be scanned in |font_file_paths|. Only used for testing.
  void SetFontFilePathsForTesting(std::vector<base::FilePath> font_file_paths) {
    font_file_paths_for_testing_ = std::move(font_file_paths);
  }

  // Override the Android build fingerprint for testing.
  void SetAndroidBuildFingerprintForTesting(
      const std::string& build_fingerprint_override) {
    android_build_fingerprint_for_testing_ = build_fingerprint_override;
  }

  // Returns the storage location of the table cache protobuf file.
  base::FilePath TableCacheFilePathForTesting() { return TableCacheFilePath(); }

 protected:
  void ScheduleLoadOrUpdateTable();

 private:

  // If an Android build fingerprint override is set through
  // SetAndroidBuildFingerprint() return that, otherwise return the actual
  // platform's Android build fingerprint.
  std::string GetAndroidBuildFingerprint() const;

  // If an override is set through SetFontFilePathsForTesting() return those
  // fonts, otherwise enumerate font files in the the Android platform font
  // directories.
  std::vector<base::FilePath> GetFontFilePaths() const;

  base::FilePath TableCacheFilePath();

  void PostCallbacks();

  // We have a asynchronous update tasks which need write access to the
  // proto_storage_ MappedReadOnlyRegion after reading the index file from disk,
  // or after scanning and indexing metadata from font files. At the same time,
  // we may receive incoming Mojo requests to tell whether the proto_storage_
  // storage area is already ready early for sync access by the
  // renderers. Synchronize the information on whether the proto_storage_ is
  // ready by means of a WaitableEvent.
  base::WaitableEvent proto_storage_ready_;
  base::MappedReadOnlyRegion proto_storage_;

  base::FilePath cache_directory_;
  std::string android_build_fingerprint_for_testing_;
  std::vector<base::FilePath> font_file_paths_for_testing_ =
      std::vector<base::FilePath>();

  struct CallbackOnTaskRunner {
    CallbackOnTaskRunner(
        scoped_refptr<base::SequencedTaskRunner>,
        blink::mojom::FontUniqueNameLookup::GetUniqueNameLookupTableCallback);
    CallbackOnTaskRunner(CallbackOnTaskRunner&&);
    ~CallbackOnTaskRunner();
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    blink::mojom::FontUniqueNameLookup::GetUniqueNameLookupTableCallback
        mojo_callback;
  };

  std::vector<CallbackOnTaskRunner> pending_callbacks_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_ANDROID_H_
