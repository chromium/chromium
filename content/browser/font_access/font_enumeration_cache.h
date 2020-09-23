// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H_

#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_MAC)
#define PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL 1
#endif

using blink::mojom::FontEnumerationStatus;

namespace content {

// A class that encapsulates building a font enumeration cache once,
// then serving the cache as a ReadOnlySharedMemoryRegion.
// Receives requests for accessing this cache from FontAccessManagerImpl
// after Mojo IPC calls from a renderer process. Per-platform subclasses are
// expected to be singletons and as such a GetInstance() method is provided as a
// convenience.
class CONTENT_EXPORT FontEnumerationCache {
 public:
  FontEnumerationCache();
  ~FontEnumerationCache();

  static FontEnumerationCache* GetInstance();

  // Enqueue a request to get notified about the availability of the shared
  // memory region holding the font enumeration cache.
  void QueueShareMemoryRegionWhenReady(
      scoped_refptr<base::TaskRunner> task_runner,
      blink::mojom::FontAccessManager::EnumerateLocalFontsCallback callback);

  // Returns whether the cache population has completed and the shared memory
  // region is ready.
  bool IsFontEnumerationCacheReady();

  void ResetStateForTesting();

 protected:
  virtual void SchedulePrepareFontEnumerationCache() = 0;

  // Retrieve the prepared memory region if it is available.
  base::ReadOnlySharedMemoryRegion DuplicateMemoryRegion();

  // Used to bind an EnumerateLocalFontsCallback to a provided TaskRunner.
  struct CallbackOnTaskRunner {
    CallbackOnTaskRunner(
        scoped_refptr<base::TaskRunner>,
        blink::mojom::FontAccessManager::EnumerateLocalFontsCallback);
    CallbackOnTaskRunner(CallbackOnTaskRunner&&);
    ~CallbackOnTaskRunner();
    scoped_refptr<base::TaskRunner> task_runner;
    blink::mojom::FontAccessManager::EnumerateLocalFontsCallback mojo_callback;
  };

  // Method to bind to callbacks_task_runner_ for execution when the font cache
  // build is complete. It will run EnumerateLocalFontsCallback on its bound
  // TaskRunner through CallbackOnTaskRunner.
  void RunPendingCallback(CallbackOnTaskRunner pending_callback);
  void StartCallbacksTaskQueue();

  bool IsFontEnumerationCacheValid() const;

  // Build the cache given a properly formed enumeration cache table.
  void BuildEnumerationCache(
      std::unique_ptr<blink::FontEnumerationTable> table);

  base::MappedReadOnlyRegion enumeration_cache_memory_;
  base::AtomicFlag enumeration_cache_built_;
  base::AtomicFlag enumeration_cache_build_started_;

  // All responses are serialized through this DeferredSequencedTaskRunner. It
  // is started when the table is ready and guarantees that requests made before
  // the table was ready are replied to first.
  scoped_refptr<base::DeferredSequencedTaskRunner> callbacks_task_runner_ =
      base::MakeRefCounted<base::DeferredSequencedTaskRunner>();

  FontEnumerationStatus status_ = FontEnumerationStatus::kOk;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H
