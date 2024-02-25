// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"

namespace blink {

unsigned DisableLayoutSideEffectsScope::count_ = 0;

}  // namespace blink
