/*
 * Copyright (C) 2006, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PLATFORM_DATA_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PLATFORM_DATA_CACHE_H_

#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

enum class AlternateFontName;
class FontCache;
class FontDataCache;
class FontDescription;
class FontFaceCreationParams;
class FontPlatformData;

// `FontPlatformDataCache` is the shared cache mapping from `FontDescription`
// to `FontPlatformData`.
class FontPlatformDataCache final {
 public:
  static std::unique_ptr<FontPlatformDataCache> Create();

  FontPlatformDataCache();
  ~FontPlatformDataCache();

  FontPlatformDataCache(const FontPlatformDataCache&) = delete;
  FontPlatformDataCache(FontPlatformDataCache&&) = delete;

  FontPlatformDataCache operator=(const FontPlatformDataCache&) = delete;
  FontPlatformDataCache operator=(FontPlatformDataCache&&) = delete;

  FontPlatformData* GetOrCreateFontPlatformData(
      FontCache* font_cache,
      const FontDescription& font_description,
      const FontFaceCreationParams& creation_params,
      AlternateFontName alternate_font_name);

  size_t ByteSize() const;
  void Clear();
  void Purge(const FontDataCache& font_data_cache);

 private:
  // `SizedFontPlatformDataSet` maps rounded font size to `FontPlatformData`.
  class SizedFontPlatformDataSet final
      : public ThreadSafeRefCounted<SizedFontPlatformDataSet> {
   public:
    static scoped_refptr<SizedFontPlatformDataSet> Create();

    ~SizedFontPlatformDataSet();

    SizedFontPlatformDataSet(const SizedFontPlatformDataSet&) = delete;
    SizedFontPlatformDataSet(SizedFontPlatformDataSet&&) = delete;

    SizedFontPlatformDataSet& operator=(const SizedFontPlatformDataSet&) =
        delete;
    SizedFontPlatformDataSet operator=(SizedFontPlatformDataSet&&) = delete;

    FontPlatformData* GetOrCreateFontPlatformData(
        FontCache* font_cache,
        const FontDescription& font_description,
        const FontFaceCreationParams& creation_params,
        float size,
        AlternateFontName alternate_font_name,
        unsigned rounded_size);

    // Returns true if `map_` is empty.
    bool Purge(const FontDataCache& font_data_cache);

    void Set(unsigned rounded_size, FontPlatformData* platform_data);

   private:
    using SizeToDataMap = HashMap<unsigned,
                                  std::unique_ptr<FontPlatformData>,
                                  WTF::IntHash<unsigned>,
                                  WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

    SizedFontPlatformDataSet();

    SizeToDataMap size_to_data_map_;
  };

  SizedFontPlatformDataSet& GetOrCreateSizeMap(const FontCacheKey& key);

  HashMap<FontCacheKey, scoped_refptr<SizedFontPlatformDataSet>> map_;

  // A maximum float value to which we limit incoming font sizes. This is the
  // smallest float so that multiplying it by
  // FontCacheKey::PrecisionMultiplier() is still smaller than
  // std::numeric_limits<unsigned>::max() - 1 in order to avoid hitting
  // HashMap sentinel values (placed at std::numeric_limits<unsigned>::max()
  // and std::numeric_limits<unsigned>::max() - 1) for
  // SizedFontPlatformDataSet and FontPlatformDataCache.
  const float font_size_limit_;

  // When true, the font size is removed from primary keys in |map_|.
  // The font size is not necessary in the primary key, because per-size
  // FontPlatformData are held in a nested map.
  // This is controlled by a base::Feature to assess impact with an
  // experiment.
  const bool no_size_in_key_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PLATFORM_DATA_CACHE_H_
