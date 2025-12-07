// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTEREST_DELAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTEREST_DELAY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT StyleInterestDelay {
  DISALLOW_NEW();

 public:
  StyleInterestDelay() = default;
  explicit StyleInterestDelay(double seconds)
      : seconds_(seconds >= 0.0 ? seconds : 0.0) {}

  double DelaySeconds() const { return seconds_; }

  bool IsNormal() const { return seconds_ < 0; }

  bool operator==(const StyleInterestDelay& other) const;

 private:
  double seconds_ = -1;
};

inline bool StyleInterestDelay::operator==(
    const StyleInterestDelay& other) const {
  return seconds_ == other.seconds_;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INTEREST_DELAY_H_
