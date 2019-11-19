// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_LOOKUP_TABLE_BUILDER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_LOOKUP_TABLE_BUILDER_WIN_H_

#include <dwrite.h>
#include <dwrite_2.h>
#include <dwrite_3.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_unique_name_table.pb.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {

// Singleton class which encapsulates building the font unique name table lookup
// once, then serving the built table as a ReadOnlySharedMemoryRegion. Receives
// requests for accessing this table from DWriteFontProxyImpl after Mojo IPC
// calls from the renderer. A method ScheduleBuildFontUniqueNameTable() is
// provided to schedule building the font unique name lookup
// structure. EnsureFontUniqueNameTable() can be called on any thread to wait
// for the lookup table to be ready. After that, DuplicateMemoryRegion() can be
// used to retrieve the lookup structure. Thread-safe when used as described
// below.
class CONTENT_EXPORT DWriteFontLookupTableBuilder {
 public:
  static DWriteFontLookupTableBuilder* GetInstance();

  // Retrieve the prepared memory region if it is available.
  // EnsureFontUniqueNameTable() must be checked before.
  base::ReadOnlySharedMemoryRegion DuplicateMemoryRegion();

  // Enqueue a request to get notified about the availability of the shared
  // memory region holding the unique font lookup table.
  // https://crbug.com/967316 shows that we do have a higher number of
  // DWriteFontProxyImpl instances, potentially running on different
  // TaskRunners. Capture each relevant task runner with a call to this method.
  void QueueShareMemoryRegionWhenReady(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      blink::mojom::DWriteFontProxy::GetUniqueNameLookupTableCallback callback);

  // Returns whether the indexing has completed and the shared memory region is
  // immediately ready without any sync operations.
  bool FontUniqueNameTableReady();

  // If needed, i.e. if we're on pre-Windows 10, posts a task to load from cache
  // or build (if cache not available) the unique name table index, should only
  // be called once at browser startup, after that,
  // QueueShareMemoryRegionWhenReady() to trigger the mojo callbacks when the
  // table is ready.
  void SchedulePrepareFontUniqueNameTableIfNeeded();

  enum class SlowDownMode { kDelayEachTask, kHangOneTask, kNoSlowdown };

  // Slow down each family indexing step for testing the internal timeout,
  // either with a single hung task or by delaying each indexing step. At the
  // same time, configure a new timeout value for testing, overriding the
  // default timeout.
  void SetSlowDownIndexingForTestingWithTimeout(SlowDownMode slowdown_mode,
                                                base::TimeDelta new_timeout);

  // Reset timeout overrides and empty table. Needed to trigger rebuilding the
  // lookup table, when testing using slowed-down indexing. Otherwise, the test
  // methods would use the already cached lookup table.
  void ResetLookupTableForTesting();

  // Resets other overrides such as the DWrite version check override and cache
  // directory back to its default values.
  void ResetStateForTesting();

  // Signals hang_event_for_testing_ which is used in testing hanging one of the
  // font name retrieval tasks.
  void ResumeFromHangForTesting();

  // Computes a hash to determine whether cache contents needed to be updated,
  // consisting of font names and their file paths read from the registry (not
  // from disk), The DWrite.dll's product version and the Chrome version, as a
  // safety mechanism to refresh the cache for every release. Exposed as a
  // public method to be able to run the hash function in a test.
  std::string ComputePersistenceHash();

  // Configures the cache directory in which to store the serialized font table
  // lookup structure. Use only in testing. Normally the directory name is
  // retrieved from ContentBrowserClient.
  void SetCacheDirectoryForTesting(base::FilePath cache_directory);

  // Configures whether the cache should be used. Needed for testing to test
  // repeated rebuilding of the font table lookup structure.
  void SetCachingEnabledForTesting(bool caching_enabled);

  // Disable DCHECKs that ensure DWriteFontLookupTableBuilder is only
  // run pre Windows 10, used for testing only to allow running the tests on
  // Windows 10.
  void OverrideDWriteVersionChecksForTesting();

 private:
  friend class base::NoDestructor<DWriteFontLookupTableBuilder>;

  struct FontFileWithUniqueNames {
    FontFileWithUniqueNames(blink::FontUniqueNameTable_UniqueFont&& font,
                            std::vector<std::string>&& names);
    ~FontFileWithUniqueNames();
    FontFileWithUniqueNames(
        DWriteFontLookupTableBuilder::FontFileWithUniqueNames&& other);
    FontFileWithUniqueNames(const FontFileWithUniqueNames&) = delete;
    FontFileWithUniqueNames& operator=(const FontFileWithUniqueNames&) = delete;

    blink::FontUniqueNameTable_UniqueFont font_entry;
    std::vector<std::string> extracted_names;
  };

  struct FamilyResult {
    FamilyResult();
    FamilyResult(FamilyResult&& other);
    ~FamilyResult();
    std::vector<FontFileWithUniqueNames> font_files_with_names;
    HRESULT exit_hresult{S_OK};

    DISALLOW_COPY_AND_ASSIGN(FamilyResult);
  };

  // Try to find a serialized lookup table from the cache directory specified at
  // construction and load it into memory.
  bool LoadFromFile();

  // Serialize the current lookup table into a file in the cache directory
  // specified at construction time.
  bool PersistToFile();

  // Initialize the cache directory from the user profile directory if
  // DWriteFontLookupTableBuilder is executed in an environment where the
  // profile is accessible.
  void InitializeCacheDirectoryFromProfile();

  // Load from cache or construct the font unique name lookup table. If the
  // cache is up to date, do not schedule a run to scan all Windows-enumerated
  // fonts.
  void PrepareFontUniqueNameTable();

  // Helper function to perform DWrite operations to retrieve path names, full
  // font name and PostScript name for a font specified by collection + family
  // index.
  static FamilyResult ExtractPathAndNamesFromFamily(
      Microsoft::WRL::ComPtr<IDWriteFontCollection> collection,
      uint32_t family_index,
      base::TimeTicks start_time,
      SlowDownMode slow_down_mode,
      base::WaitableEvent* hang_event_for_testing,
      base::TimeDelta indexing_timeout);

  // Callback from scheduled tasks to add the retrieved font names to the
  // protobuf.
  void AppendFamilyResultAndFinalizeIfNeeded(const FamilyResult& family_result);

  // Sort the results that were collected into the protobuf structure and signal
  // that font unique name lookup table construction is complete. Serializes the
  // constructed protobuf to disk.
  void FinalizeFontTable();

  void OnTimeout();

  bool IsFontUniqueNameTableValid();

  void InitializeDirectWrite();

  base::FilePath TableCacheFilePath();

  // Returns true if IDWriteFactory3 is available, which means that we can
  // access IDWriteFontSet API which provides direct lookup by PostScript name
  // and full font name, in which case we do not need to build this table.
  bool HasDWriteUniqueFontLookups();

  base::TimeDelta IndexingTimeout();

  void PostCallbacks();

  DWriteFontLookupTableBuilder();
  ~DWriteFontLookupTableBuilder();

  // Protobuf structure temporarily used and shared during table construction.
  std::unique_ptr<blink::FontUniqueNameTable> font_unique_name_table_;

  base::MappedReadOnlyRegion font_table_memory_;
  base::WaitableEvent font_table_built_;

  bool direct_write_initialized_ = false;
  base::TimeDelta font_indexing_timeout_;
  Microsoft::WRL::ComPtr<IDWriteFontCollection> collection_;
  Microsoft::WRL::ComPtr<IDWriteFactory2> factory2_;
  Microsoft::WRL::ComPtr<IDWriteFactory3> factory3_;
  SlowDownMode slow_down_mode_for_testing_ = SlowDownMode::kNoSlowdown;
  uint32_t outstanding_family_results_ = 0;
  uint32_t family_results_non_empty_ = 0;
  uint32_t family_results_empty_ = 0;
  base::TimeTicks start_time_table_ready_;
  base::TimeTicks start_time_table_build_;
  base::FilePath cache_directory_;

  bool caching_enabled_ = true;
  base::Optional<base::WaitableEvent> hang_event_for_testing_;
  base::CancelableOnceCallback<void()> timeout_callback_;

  struct CallbackOnTaskRunner {
    CallbackOnTaskRunner(
        scoped_refptr<base::SequencedTaskRunner>,
        blink::mojom::DWriteFontProxy::GetUniqueNameLookupTableCallback);
    CallbackOnTaskRunner(CallbackOnTaskRunner&&);
    ~CallbackOnTaskRunner();
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    blink::mojom::DWriteFontProxy::GetUniqueNameLookupTableCallback
        mojo_callback;
  };

  std::vector<CallbackOnTaskRunner> pending_callbacks_;
  std::map<HRESULT, unsigned> scanning_error_reasons_;

  DISALLOW_COPY_AND_ASSIGN(DWriteFontLookupTableBuilder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_LOOKUP_TABLE_BUILDER_WIN_H_
