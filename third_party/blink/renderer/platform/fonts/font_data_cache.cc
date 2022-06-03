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

#include "third_party/blink/renderer/platform/fonts/font_data_cache.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace blink {

#if !defined(OS_ANDROID)
const unsigned kCMaxInactiveFontData = 250;
const unsigned kCTargetInactiveFontData = 200;
#else
const unsigned kCMaxInactiveFontData = 225;
const unsigned kCTargetInactiveFontData = 200;
#endif

scoped_refptr<SimpleFontData> FontDataCache::Get(const FontPlatformData* platform_data,
                                          ShouldRetain should_retain,
                                          bool subpixel_ascent_descent) {
  if (!platform_data)
    return nullptr;

  // TODO: crbug.com/446376 - This should not happen, but we currently
  // do not have a reproduction for the crash that an empty typeface()
  // causes downstream from here.
  if (!platform_data->Typeface()) {
    DLOG(ERROR)
        << "Empty typeface() in FontPlatformData when accessing FontDataCache.";
    return nullptr;
  }

  Cache::iterator result = cache_.find(platform_data);
  if (result == cache_.end()) {
    std::pair<scoped_refptr<SimpleFontData>, unsigned> new_value(
        SimpleFontData::Create(*platform_data, nullptr,
                               subpixel_ascent_descent),
        should_retain == kRetain ? 1 : 0);
    // The new SimpleFontData takes a copy of the incoming FontPlatformData
    // object. The incoming key may be temporary. So, for cache storage, take
    // the address of the newly created FontPlatformData that is copied an owned
    // by SimpleFontData.
    cache_.Set(&new_value.first->PlatformData(), new_value);
    if (should_retain == kDoNotRetain)
      inactive_font_data_.insert(new_value.first);
    return std::move(new_value.first);
  }

  if (!result.Get()->value.second) {
    DCHECK(inactive_font_data_.Contains(result.Get()->value.first));
    inactive_font_data_.erase(result.Get()->value.first);
  }

  if (should_retain == kRetain) {
    result.Get()->value.second++;
  } else if (!result.Get()->value.second) {
    // If shouldRetain is DoNotRetain and count is 0, we want to remove the
    // fontData from m_inactiveFontData (above) and re-add here to update LRU
    // position.
    inactive_font_data_.insert(result.Get()->value.first);
  }

  return result.Get()->value.first;
}

bool FontDataCache::Contains(const FontPlatformData* font_platform_data) const {
  return cache_.Contains(font_platform_data);
}

void FontDataCache::Release(const SimpleFontData* font_data) {
  DCHECK(!font_data->IsCustomFont());

  Cache::iterator it = cache_.find(&(font_data->PlatformData()));
  DCHECK_NE(it, cache_.end());
  if (it == cache_.end())
    return;

  DCHECK(it->value.second);
  if (!--it->value.second)
    inactive_font_data_.insert(it->value.first);
}

bool FontDataCache::Purge(PurgeSeverity purge_severity) {
  if (purge_severity == kForcePurge)
    return PurgeLeastRecentlyUsed(INT_MAX);

  if (inactive_font_data_.size() > kCMaxInactiveFontData)
    return PurgeLeastRecentlyUsed(inactive_font_data_.size() -
                                  kCTargetInactiveFontData);

  return false;
}

bool FontDataCache::PurgeLeastRecentlyUsed(int count) {
  // Guard against reentry when e.g. a deleted FontData releases its small caps
  // FontData.
  static bool is_purging;
  if (is_purging)
    return false;

  is_purging = true;

  Vector<scoped_refptr<SimpleFontData>, 20> font_data_to_delete;
  auto end = inactive_font_data_.end();
  auto it = inactive_font_data_.begin();
  for (int i = 0; i < count && it != end; ++it, ++i) {
    const scoped_refptr<SimpleFontData>& font_data = *it;
    cache_.erase(&(font_data->PlatformData()));
    // We should not delete SimpleFontData here because deletion can modify
    // m_inactiveFontData. See http://trac.webkit.org/changeset/44011
    font_data_to_delete.push_back(font_data);
  }

  if (it == end) {
    // Removed everything
    inactive_font_data_.clear();
  } else {
    for (int i = 0; i < count; ++i)
      inactive_font_data_.erase(inactive_font_data_.begin());
  }

  bool did_work = font_data_to_delete.size();

  font_data_to_delete.clear();

  is_purging = false;

  return did_work;
}

}  // namespace blink
