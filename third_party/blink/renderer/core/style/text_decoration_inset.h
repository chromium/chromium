// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_DECORATION_INSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_DECORATION_INSET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

// https://drafts.csswg.org/css-text-decor-4/#text-decoration-inset-property
class CORE_EXPORT TextDecorationInset {
 public:
  TextDecorationInset() = default;
  TextDecorationInset(const Length& start, const Length& end)
      : start_(start), end_(end) {}

  const Length& GetStart() const { return start_; }
  const Length& GetEnd() const { return end_; }

  bool operator==(const TextDecorationInset&) const = default;

 private:
  Length start_;
  Length end_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_DECORATION_INSET_H_
