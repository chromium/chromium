// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_MAP_H_

#include "third_party/blink/renderer/platform/fonts/font_cache_client.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"
#include "third_party/blink/renderer/platform/fonts/lock_for_parallel_text_shaping.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

class FontSelector;

namespace blink {

class PLATFORM_EXPORT FontFallbackMap : public FontCacheClient,
                                        public FontSelectorClient {
 public:
  explicit FontFallbackMap(FontSelector* font_selector)
      : font_selector_(font_selector) {}

  ~FontFallbackMap() override;

  FontSelector* GetFontSelector() const { return font_selector_; }

  scoped_refptr<FontFallbackList> Get(const FontDescription& font_description)
      LOCKS_EXCLUDED(lock_);

  void Remove(const FontDescription& font_description) LOCKS_EXCLUDED(lock_);

  void Trace(Visitor* visitor) const override;

 private:
  // FontSelectorClient
  void FontsNeedUpdate(FontSelector*, FontInvalidationReason) override
      LOCKS_EXCLUDED(lock_);

  // FontCacheClient
  void FontCacheInvalidated() override LOCKS_EXCLUDED(lock_);

  void InvalidateAll() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  template <typename Predicate>
  void InvalidateInternal(Predicate predicate) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  Member<FontSelector> font_selector_;
  mutable LockForParallelTextShaping lock_;
  HashMap<FontDescription, scoped_refptr<FontFallbackList>>
      fallback_list_for_description_ GUARDED_BY(lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_MAP_H_
