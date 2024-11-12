// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_ENUMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_ENUMS_H_

namespace blink {

enum class CSSGapDecorationPropertyType {
  kColor,
  // TODO(crbug.com/357648037): Add kStyle and kWidth when implemented.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GAP_DECORATION_PROPERTY_ENUMS_H_
