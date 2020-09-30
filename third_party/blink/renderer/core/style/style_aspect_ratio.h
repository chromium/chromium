// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_ASPECT_RATIO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_ASPECT_RATIO_H_

#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

enum class EAspectRatioType { kAuto, kAutoAndRatio, kRatio };

class StyleAspectRatio {
  DISALLOW_NEW();

 public:
  // Style data for aspect-ratio: auto || <ratio>
  StyleAspectRatio(EAspectRatioType type, FloatSize ratio)
      : type_(static_cast<unsigned>(type)), ratio_(ratio) {}

  // 0/x and x/0 are valid (and computed style needs to serialize them
  // as such), but they are not useful for layout, so we map it to auto here.
  EAspectRatioType GetType() const {
    if (ratio_.Width() == 0 || ratio_.Height() == 0)
      return EAspectRatioType::kAuto;
    return GetTypeForComputedStyle();
  }

  EAspectRatioType GetTypeForComputedStyle() const {
    return static_cast<EAspectRatioType>(type_);
  }

  bool IsAuto() const { return GetType() == EAspectRatioType::kAuto; }

  FloatSize GetRatio() const { return ratio_; }

  bool operator==(const StyleAspectRatio& o) const {
    return type_ == o.type_ && ratio_ == o.ratio_;
  }

  bool operator!=(const StyleAspectRatio& o) const { return !(*this == o); }

 private:
  unsigned type_ : 2;  // EAspectRatioType
  FloatSize ratio_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_ASPECT_RATIO_H_
