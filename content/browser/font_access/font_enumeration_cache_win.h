// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_WIN_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_WIN_H_

#include <dwrite.h>
#include <stdint.h>
#include <wrl/client.h>

#include <map>
#include <memory>
#include <string>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

// Windows implementation of FontEnumerationCache.
// Calls DirectWrite font APIs. Requires Windows 7 with KB2670838 and newer.
class CONTENT_EXPORT FontEnumerationCacheWin : public FontEnumerationCache {
 public:
  // The constructor is public for internal use of base::SequenceBound.
  //
  // Production code should call FontEnumerationCache::Create(). Testing code
  // should call FontEnumerationCache::CreateForTesting().
  FontEnumerationCacheWin(absl::optional<std::string> locale_override,
                          base::PassKey<FontEnumerationCache>);

  FontEnumerationCacheWin(const FontEnumerationCacheWin&) = delete;
  FontEnumerationCacheWin& operator=(const FontEnumerationCacheWin&) = delete;

  ~FontEnumerationCacheWin() override;

  // A data structure to hold font family results from DirectWrite.
  struct FamilyDataResult {
    std::vector<blink::FontEnumerationTable_FontMetadata> fonts;
    FamilyDataResult();
    FamilyDataResult(const FamilyDataResult&) = delete;
    FamilyDataResult& operator=(const FamilyDataResult&) = delete;
    ~FamilyDataResult();
  };

 protected:
  // FontEnumerationCache:
  blink::FontEnumerationTable ComputeFontEnumerationData(
      const std::string& locale) override;

 private:
  void PrepareFontEnumerationCache();

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_WIN_H_
