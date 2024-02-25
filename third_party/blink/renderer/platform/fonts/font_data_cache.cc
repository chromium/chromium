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

#include "base/auto_reset.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace blink {

namespace {

// The maximum number of strong references to retain via the LRU.
// This explicitly leaks fonts (and related objects) unless under extreme
// memory pressure where it will be cleared. DO NOT increase unnecessarily.
const wtf_size_t kMaxSize = 64;

}  // namespace

const SimpleFontData* FontDataCache::Get(const FontPlatformData* platform_data,
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

  auto add_result = cache_.insert(platform_data, nullptr);
  if (add_result.is_new_entry) {
    add_result.stored_value->value = MakeGarbageCollected<SimpleFontData>(
        platform_data, nullptr, subpixel_ascent_descent);
  }

  const SimpleFontData* result = add_result.stored_value->value;

  // Update our LRU to keep a strong reference to `result`.
  strong_reference_lru_.PrependOrMoveToFirst(result);
  while (strong_reference_lru_.size() > kMaxSize) {
    strong_reference_lru_.pop_back();
  }

  return result;
}

}  // namespace blink
