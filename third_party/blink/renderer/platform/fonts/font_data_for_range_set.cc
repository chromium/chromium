// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_data_for_range_set.h"

#include "third_party/blink/renderer/platform/fonts/font_cache.h"

namespace blink {

FontDataForRangeSet::FontDataForRangeSet(const FontDataForRangeSet& other) {
  font_data_ = other.font_data_;
  range_set_ = other.range_set_;
}

FontDataForRangeSetFromCache::~FontDataForRangeSetFromCache() {
  if (font_data_ && !font_data_->IsCustomFont()) {
    FontCache::Get().ReleaseFontData(font_data_.get());
  }
}

}  // namespace blink
