// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H_

#include <memory>
#include <string>

#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_MAC)
#define PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL 1
#endif

namespace content {

// A class that encapsulates building a font enumeration cache once,
// then serving the cache as a ReadOnlySharedMemoryRegion.
// Receives requests for accessing this cache from FontAccessManagerImpl
// after Mojo IPC calls from a renderer process.
class CONTENT_EXPORT FontEnumerationCache {
 public:
  // Factory method for production instances.
  static base::SequenceBound<FontEnumerationCache> Create();

  // Factory method with dependency injection support for testing.
  //
  // `task_runner` must allow blocking tasks.
  static base::SequenceBound<FontEnumerationCache> CreateForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      absl::optional<std::string> locale_override);

  FontEnumerationCache(const FontEnumerationCache&) = delete;
  FontEnumerationCache& operator=(const FontEnumerationCache&) = delete;

  virtual ~FontEnumerationCache();

  using CacheTaskCallback =
      base::OnceCallback<void(blink::mojom::FontEnumerationStatus,
                              base::ReadOnlySharedMemoryRegion)>;

  // Enqueue a request to get notified about the availability of the shared
  // memory region holding the font enumeration cache.
  void QueueShareMemoryRegionWhenReady(
      scoped_refptr<base::TaskRunner> task_runner,
      CacheTaskCallback callback);

 protected:
  // The constructor is intentionally only exposed to subclasses. Production
  // code must use the Create() factory method.
  explicit FontEnumerationCache(absl::optional<std::string> locale_override);

  virtual void SchedulePrepareFontEnumerationCache() = 0;

  // Retrieve the prepared memory region if it is available.
  base::ReadOnlySharedMemoryRegion DuplicateMemoryRegion();

  // Used to bind a CacheTaskCallback to a provided TaskRunner.
  struct CallbackOnTaskRunner {
    CallbackOnTaskRunner(scoped_refptr<base::TaskRunner>, CacheTaskCallback);
    CallbackOnTaskRunner(CallbackOnTaskRunner&&);
    ~CallbackOnTaskRunner();
    scoped_refptr<base::TaskRunner> task_runner;
    CacheTaskCallback callback;
  };

  // Method to bind to callbacks_task_runner_ for execution when the font cache
  // build is complete. It will run CacheTaskCallback on its bound
  // TaskRunner through CallbackOnTaskRunner.
  void RunPendingCallback(CallbackOnTaskRunner pending_callback);
  void StartCallbacksTaskQueue();

  bool IsFontEnumerationCacheValid() const;

  // Returns whether the cache population has completed and the shared memory
  // region is ready.
  bool IsFontEnumerationCacheReady();

  // Build the cache given a properly formed enumeration cache table.
  void BuildEnumerationCache(
      std::unique_ptr<blink::FontEnumerationTable> table);

  const absl::optional<std::string> locale_override_;

  base::MappedReadOnlyRegion enumeration_cache_memory_;
  std::unique_ptr<base::AtomicFlag> enumeration_cache_built_;
  std::unique_ptr<base::AtomicFlag> enumeration_cache_build_started_;

  // All responses are serialized through this DeferredSequencedTaskRunner. It
  // is started when the table is ready and guarantees that requests made before
  // the table was ready are replied to first.
  scoped_refptr<base::DeferredSequencedTaskRunner> callbacks_task_runner_ =
      base::MakeRefCounted<base::DeferredSequencedTaskRunner>();

  blink::mojom::FontEnumerationStatus status_ =
      blink::mojom::FontEnumerationStatus::kOk;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H_
