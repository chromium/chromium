// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATION_UTILS_H_

namespace blink {

class LayoutView;

float CalculateOverflowShrinkForPrinting(const LayoutView&,
                                         float maximum_shrink_factor);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATION_UTILS_H_
