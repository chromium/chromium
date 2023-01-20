/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_DATA_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_DATA_CACHE_H_

#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

namespace blink {

enum ShouldRetain { kRetain, kDoNotRetain };
enum PurgeSeverity { kPurgeIfNeeded, kForcePurge };

struct FontDataCacheKeyHashTraits : GenericHashTraits<const FontPlatformData*> {
  STATIC_ONLY(FontDataCacheKeyHashTraits);
  static unsigned GetHash(const FontPlatformData* platform_data) {
    return platform_data->GetHash();
  }

  static bool Equal(const FontPlatformData* a, const FontPlatformData* b) {
    return *a == *b;
  }

  static constexpr bool kSafeToCompareToEmptyOrDeleted = false;
};

class FontDataCache final {
  USING_FAST_MALLOC(FontDataCache);

 public:
  static std::unique_ptr<FontDataCache> Create();

  FontDataCache() = default;
  FontDataCache(const FontDataCache&) = delete;
  FontDataCache& operator=(const FontDataCache&) = delete;

  scoped_refptr<SimpleFontData> Get(const FontPlatformData*,
                                    ShouldRetain = kRetain,
                                    bool subpixel_ascent_descent = false);
  bool Contains(const FontPlatformData*) const;
  void Release(const SimpleFontData*);

  // Purges items in FontDataCache according to provided severity.
  // Returns true if any removal of cache items actually occurred.
  bool Purge(PurgeSeverity);

 private:
  bool PurgeLeastRecentlyUsed(int count);

  typedef HashMap<const FontPlatformData*,
                  std::pair<scoped_refptr<SimpleFontData>, unsigned>,
                  FontDataCacheKeyHashTraits>
      Cache;

  Cache cache_;
  LinkedHashSet<scoped_refptr<SimpleFontData>> inactive_font_data_;
  bool is_purging_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_DATA_CACHE_H_
