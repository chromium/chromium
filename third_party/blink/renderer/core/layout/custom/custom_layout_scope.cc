// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_scope.h"

namespace blink {

CustomLayoutScope* CustomLayoutScope::current_scope_ = nullptr;

}  // namespace blink
