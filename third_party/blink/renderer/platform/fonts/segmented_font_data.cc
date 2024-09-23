/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/segmented_font_data.h"

#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

const SimpleFontData* SegmentedFontData::FontDataForCharacter(UChar32 c) const {
  for (const auto& face : faces_) {
    if (face->Contains(c)) {
      return face->FontData();
    }
  }
  return faces_[0]->FontData();
}

bool SegmentedFontData::ContainsCharacter(UChar32 c) const {
  for (const auto& face : faces_) {
    if (face->Contains(c)) {
      return true;
    }
  }
  return false;
}

bool SegmentedFontData::IsCustomFont() const {
  // All segmented fonts are custom fonts.
  return true;
}

bool SegmentedFontData::IsLoading() const {
  for (const auto& face : faces_) {
    if (face->FontData()->IsLoading()) {
      return true;
    }
  }
  return false;
}

// Returns true if any of the sub fonts are loadingFallback.
bool SegmentedFontData::IsLoadingFallback() const {
  for (const auto& face : faces_) {
    if (face->FontData()->IsLoadingFallback()) {
      return true;
    }
  }
  return false;
}

bool SegmentedFontData::IsSegmented() const {
  return true;
}

bool SegmentedFontData::ShouldSkipDrawing() const {
  for (const auto& face : faces_) {
    if (face->FontData()->ShouldSkipDrawing()) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
