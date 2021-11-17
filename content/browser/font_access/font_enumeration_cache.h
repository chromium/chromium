// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H_

#include <memory>
#include <string>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_MAC) || defined(OS_FUCHSIA)
#define PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL 1
#endif

namespace content {

struct CONTENT_EXPORT FontEnumerationData {
  blink::mojom::FontEnumerationStatus status;

  // Stores a serialized blink::FontEnumerationTable proto.
  //
  // Valid iff `status` is kOk.
  base::ReadOnlySharedMemoryRegion font_data;
};

// Caches font enumeration data, and serves it as a ReadOnlySharedMemoryRegion.
//
// This class is not thread-safe. Each instance must be accessed from a single
// sequence, which must allow blocking.
class CONTENT_EXPORT FontEnumerationCache {
 public:
  // Factory method for production instances.
  static base::SequenceBound<FontEnumerationCache> Create();

  // Factory method with dependency injection support for testing.
  //
  // `task_runner` must allow blocking.
  static base::SequenceBound<FontEnumerationCache> CreateForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      absl::optional<std::string> locale_override);

  FontEnumerationCache(const FontEnumerationCache&) = delete;
  FontEnumerationCache& operator=(const FontEnumerationCache&) = delete;

  virtual ~FontEnumerationCache();

  FontEnumerationData GetFontEnumerationData();

 protected:
  // The constructor is intentionally only exposed to subclasses. Production
  // code must use the Create() factory method.
  explicit FontEnumerationCache(absl::optional<std::string> locale_override);

  virtual blink::FontEnumerationTable ComputeFontEnumerationData(
      const std::string& locale) = 0;

 private:
  // Build the cache given a properly formed enumeration cache table.
  void BuildEnumerationCache(blink::FontEnumerationTable& table);

  SEQUENCE_CHECKER(sequence_checker_);

  // nullopt in production. Only set in testing.
  const absl::optional<std::string> locale_override_;

  bool initialized_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Valid only if `initialized_` is true.
  FontEnumerationData data_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_H_
