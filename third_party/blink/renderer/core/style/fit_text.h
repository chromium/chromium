// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

enum class FitTextType : uint8_t {
  kNone,
  kGrow,
  kShrink,
};

enum class FitTextTarget : uint8_t {
  kConsistent,
  kPerLine,
  kPerLineAll,
};

enum class FitTextMethod : uint8_t {
  kScale,     // Paint-time scaling. This is the default method.
  kFontSize,  // Reshaping.
};

class CORE_EXPORT FitText {
  DISALLOW_NEW();

 public:
  FitText() = default;
  FitText(FitTextType type, FitTextTarget target, std::optional<float> limit)
      : type_(type), target_(target), scale_factor_limit_(limit) {}

  bool operator==(const FitText& other) const {
    return type_ == other.type_ && target_ == other.target_ &&
           scale_factor_limit_ == other.scale_factor_limit_;
  }

  FitTextType Type() const { return type_; }
  FitTextTarget Target() const { return target_; }
  FitTextMethod Method() const;
  // This returns 1.0 for "100%".
  std::optional<float> ScaleFactorLimit() const { return scale_factor_limit_; }

  // A debug helper.
  String ToString() const;

 private:
  FitTextType type_ = FitTextType::kNone;
  FitTextTarget target_ = FitTextTarget::kConsistent;
  std::optional<float> scale_factor_limit_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TEXT_H_
