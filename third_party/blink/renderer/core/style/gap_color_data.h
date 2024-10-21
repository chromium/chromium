// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_COLOR_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_COLOR_DATA_H_

#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

typedef HeapVector<StyleColor, 1> StyleColorVector;

struct CORE_EXPORT StyleColorRepeater
    : public GarbageCollected<StyleColorRepeater> {
 public:
  StyleColorRepeater() = default;
  explicit StyleColorRepeater(StyleColorVector repeated_colors);
  StyleColorRepeater(StyleColorVector repeated_colors, wtf_size_t repeat_count);

  bool operator==(const StyleColorRepeater& other) const;

  bool IsAutoRepeater() const { return !repeat_count_.has_value(); }
  const StyleColorVector& RepeatedColors() const { return repeated_colors_; }
  wtf_size_t RepeatCount() const {
    CHECK(repeat_count_.has_value());
    return repeat_count_.value();
  }

  void Trace(Visitor* visitor) const { visitor->Trace(repeated_colors_); }

 private:
  StyleColorVector repeated_colors_;
  std::optional<wtf_size_t> repeat_count_ = std::nullopt;
};

// A GapColorData is a single StyleColor or a StyleColorRepeater.
class CORE_EXPORT GapColorData {
  DISALLOW_NEW();

 public:
  GapColorData() = default;
  explicit GapColorData(StyleColor gap_color);
  explicit GapColorData(StyleColorRepeater* color_repeater);
  void Trace(Visitor* visitor) const {
    visitor->Trace(gap_color_);
    visitor->Trace(color_repeater_);
  }

  bool operator==(const GapColorData& other) const;

  const StyleColor GetGapColor() const {
    CHECK(!color_repeater_);
    return gap_color_;
  }

  const StyleColorRepeater* GetColorRepeater() const {
    CHECK(color_repeater_);
    return color_repeater_.Get();
  }

  bool IsRepeaterData() const { return color_repeater_ != nullptr; }

 private:
  StyleColor gap_color_;
  Member<StyleColorRepeater> color_repeater_;
};

typedef HeapVector<GapColorData, 1> GapDataVector;

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::GapColorData)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_COLOR_DATA_H_
