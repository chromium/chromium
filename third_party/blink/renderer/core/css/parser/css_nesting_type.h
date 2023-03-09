// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_NESTING_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_NESTING_TYPE_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

enum class CSSNestingType {
  // We are not in a nesting context, and '&' resolves like :scope instead.
  kNone,
  // We are in a css-nesting nesting context, and '&' resolves according to:
  // https://drafts.csswg.org/css-nesting-1/#nest-selector
  kNesting,
  // TOOD(crbug.com/1280240): Add kScope
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_NESTING_TYPE_H_
