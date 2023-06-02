// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_scrollbar_color.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

StyleScrollbarColor::StyleScrollbarColor(StyleColor thumbColor,
                                         StyleColor trackColor)
    : thumb_color_(thumbColor), track_color_(trackColor) {}

}  // namespace blink
