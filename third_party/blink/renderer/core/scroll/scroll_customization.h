// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_CUSTOMIZATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_CUSTOMIZATION_H_

#include <stdint.h>

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {
namespace scroll_customization {
using ScrollDirection = uint8_t;

constexpr ScrollDirection kScrollDirectionNone = 0;
constexpr ScrollDirection kScrollDirectionPanLeft = 1 << 0;
constexpr ScrollDirection kScrollDirectionPanRight = 1 << 1;
constexpr ScrollDirection kScrollDirectionPanX =
    kScrollDirectionPanLeft | kScrollDirectionPanRight;
constexpr ScrollDirection kScrollDirectionPanUp = 1 << 2;
constexpr ScrollDirection kScrollDirectionPanDown = 1 << 3;
constexpr ScrollDirection kScrollDirectionPanY =
    kScrollDirectionPanUp | kScrollDirectionPanDown;
constexpr ScrollDirection kScrollDirectionAuto =
    kScrollDirectionPanX | kScrollDirectionPanY;

}  // namespace scroll_customization
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_CUSTOMIZATION_H_
