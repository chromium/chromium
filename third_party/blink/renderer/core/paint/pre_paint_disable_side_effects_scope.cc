// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/pre_paint_disable_side_effects_scope.h"

namespace blink {

unsigned PrePaintDisableSideEffectsScope::count_ = 0;

}  // namespace blink
