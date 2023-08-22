// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_base_data.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

StyleBaseData::StyleBaseData(const ComputedStyle* style,
                             std::unique_ptr<CSSBitset> set)
    : computed_style_(style), important_set_(std::move(set)) {}

}  // namespace blink
