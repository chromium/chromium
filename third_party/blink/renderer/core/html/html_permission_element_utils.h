// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_UTILS_H_

#include <optional>

#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

class HTMLPermissionElementUtils {
 public:
  // Returns a calculation expression node that represents the given length
  // bounded by the given lower and upper bounds using clamp/min/max based on
  // which bounds are specified.
  static const CalculationExpressionNode* BuildLengthBoundExpr(
      const Length& length,
      const CalculationExpressionNode* lower_bound_expr,
      const CalculationExpressionNode* upper_bound_expr);

  // Returns an adjusted bounded length that takes in the site-provided length
  // and creates an expression-type length that is bounded on upper or lower
  // sides by the provided bounds. The expression uses min|max|clamp depending
  // on which bound(s) is/are present. The bounds will be multiplied by
  // |fit-content-size| if |should_multiply_by_content_size| is true. At least
  // one of the bounds must be specified.
  //
  // If |length| is not a "specified" length, it is ignored and the returned
  // length will be |lower_bound| or |upper_bound| (if both are specified,
  // |lower_bound| is used), optionally multiplied by |fit-content-size| as
  // described above.
  static Length AdjustedBoundedLength(const Length& length,
                                      std::optional<float> lower_bound,
                                      std::optional<float> upper_bound,
                                      bool should_multiply_by_content_size);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_UTILS_H_
