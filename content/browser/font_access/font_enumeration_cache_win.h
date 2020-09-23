// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_WIN_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_WIN_H_

#include <dwrite.h>
#include <wrl.h>

#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

using blink::mojom::FontEnumerationStatus;

namespace base {
template <typename T>
class NoDestructor;

class ElapsedTimer;
}

namespace content {

// Windows implementation of FontEnumerationCache.
// Calls DirectWrite font APIs. Requires Windows 7 with KB2670838 and newer.
class CONTENT_EXPORT FontEnumerationCacheWin : public FontEnumerationCache {
 public:
  FontEnumerationCacheWin();
  ~FontEnumerationCacheWin();

  // Disallow copy and assign.
  FontEnumerationCacheWin(const FontEnumerationCacheWin&) = delete;
  FontEnumerationCacheWin operator=(const FontEnumerationCacheWin&) = delete;

  // A data structure to hold font family results from DirectWrite.
  struct FamilyDataResult {
    std::vector<blink::FontEnumerationTable_FontMetadata> fonts;
    HRESULT exit_hresult{S_OK};
    FamilyDataResult();
    ~FamilyDataResult();

    DISALLOW_COPY_AND_ASSIGN(FamilyDataResult);
  };
  static FontEnumerationCacheWin* GetInstance();

 protected:
  // FontEnumerationCache interface.
  void SchedulePrepareFontEnumerationCache() override;

 private:
  friend class base::NoDestructor<FontEnumerationCacheWin>;
  // This gives FontEnumerationCache::GetInstance access to the class
  // constructor.
  friend class FontEnumerationCache;

  void InitializeDirectWrite();
  void PrepareFontEnumerationCache();
  void AppendFontDataAndFinalizeIfNeeded(
      std::unique_ptr<FamilyDataResult> family_data_result);
  void FinalizeEnumerationCache();

  bool direct_write_initialized_ = false;
  Microsoft::WRL::ComPtr<IDWriteFontCollection> collection_;
  uint32_t outstanding_family_results_ = 0;

  // Protobuf structure temporarily used during cache construction and shared.
  std::unique_ptr<blink::FontEnumerationTable> font_enumeration_table_;

  std::map<HRESULT, unsigned> enumeration_errors_;
  std::unique_ptr<base::ElapsedTimer> enumeration_timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_WIN_H_
