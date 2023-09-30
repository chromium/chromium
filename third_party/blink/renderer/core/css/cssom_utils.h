// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_UTILS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CSSValue;

class CSSOMUtils {
  STATIC_ONLY(CSSOMUtils);

 public:
  static bool IncludeDependentGridLineEndValue(const CSSValue* line_start,
                                               const CSSValue* line_end);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_UTILS_H_
