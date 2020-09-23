// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_MAC_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_MAC_H_

#include "base/memory/scoped_refptr.h"
#include "base/task_runner.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/common/content_export.h"

using blink::mojom::FontEnumerationStatus;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {

// Mac implementation of FontEnumerationCache.
class CONTENT_EXPORT FontEnumerationCacheMac : public FontEnumerationCache {
 public:
  FontEnumerationCacheMac();
  ~FontEnumerationCacheMac();

  // Disallow copy and assign.
  FontEnumerationCacheMac(const FontEnumerationCacheMac&) = delete;
  FontEnumerationCacheMac operator=(const FontEnumerationCacheMac&) = delete;

  static FontEnumerationCacheMac* GetInstance();

 protected:
  // FontEnumerationCache interface.
  void SchedulePrepareFontEnumerationCache() override;

 private:
  friend class base::NoDestructor<FontEnumerationCacheMac>;
  // This gives FontEnumerationCache::GetInstance access to the class
  // constructor.
  friend class FontEnumerationCache;

  void PrepareFontEnumerationCache();
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_MAC_H_
