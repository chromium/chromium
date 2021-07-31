// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_MAC_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_MAC_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/task_runner.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Mac implementation of FontEnumerationCache.
class CONTENT_EXPORT FontEnumerationCacheMac : public FontEnumerationCache {
 public:
  // The constructor is public for internal use of base::SequenceBound.
  //
  // Production code should call FontEnumerationCache::Create(). Testing code
  // should call FontEnumerationCache::CreateForTesting().
  FontEnumerationCacheMac(absl::optional<std::string> locale_override,
                          base::PassKey<FontEnumerationCache>);

  FontEnumerationCacheMac(const FontEnumerationCacheMac&) = delete;
  FontEnumerationCacheMac& operator=(const FontEnumerationCacheMac&) = delete;

  ~FontEnumerationCacheMac() override;

 protected:
  // FontEnumerationCache:
  void SchedulePrepareFontEnumerationCache() override;

 private:
  void PrepareFontEnumerationCache();
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_MAC_H_
