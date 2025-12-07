// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

enum class FitTextTarget : uint8_t {
  kNone,
  kPerLine,
  kConsistent,
};

enum class FitTextMethod : uint8_t {
  kScale,  // This is the default method.
  kFontSize,
  kScaleInline,
  kLetterSpacing,
};

class CORE_EXPORT FitText {
  DISALLOW_NEW();

 public:
  FitText() = default;
  FitText(FitTextTarget target,
          std::optional<FitTextMethod> method,
          std::optional<float> size_limit)
      : target_(target), method_(method), size_limit_(size_limit) {}

  bool operator==(const FitText& other) const {
    return target_ == other.target_ && method_ == other.method_ &&
           size_limit_ == other.size_limit_;
  }

  FitTextTarget Target() const { return target_; }
  FitTextMethod Method() const {
    return method_.value_or(FitTextMethod::kScale);
  }
  std::optional<float> SizeLimit() const { return size_limit_; }

  // A debug helper.
  String ToString() const;

 private:
  FitTextTarget target_ = FitTextTarget::kNone;
  std::optional<FitTextMethod> method_;
  std::optional<float> size_limit_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TEXT_H_
