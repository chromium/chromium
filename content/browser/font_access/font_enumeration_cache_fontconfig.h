// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_FONTCONFIG_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_FONTCONFIG_H_

#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/common/content_export.h"

using blink::mojom::FontEnumerationStatus;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {

// ChromeOS and Linux implementation of FontEnumerationCache.
class CONTENT_EXPORT FontEnumerationCacheFontconfig
    : public FontEnumerationCache {
 public:
  FontEnumerationCacheFontconfig();
  ~FontEnumerationCacheFontconfig();

  // Disallow copy and assign.
  FontEnumerationCacheFontconfig(const FontEnumerationCacheFontconfig&) =
      delete;
  FontEnumerationCacheFontconfig operator=(
      const FontEnumerationCacheFontconfig&) = delete;

  static FontEnumerationCacheFontconfig* GetInstance();

 protected:
  // FontEnumerationCache interface.
  void SchedulePrepareFontEnumerationCache() override;

 private:
  friend class base::NoDestructor<FontEnumerationCacheFontconfig>;
  // This gives FontEnumerationCache::GetInstance access to the class
  // constructor.
  friend class FontEnumerationCache;

  void PrepareFontEnumerationCache();
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_FONTCONFIG_H_
