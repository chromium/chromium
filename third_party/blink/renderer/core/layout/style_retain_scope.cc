// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/style_retain_scope.h"

#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

ABSL_CONST_INIT thread_local StyleRetainScope* current = nullptr;

}  // namespace

StyleRetainScope::StyleRetainScope() : resetter_(&current, this) {}

StyleRetainScope::~StyleRetainScope() {
  DCHECK_EQ(current, this);
}

StyleRetainScope* StyleRetainScope::Current() {
  return current;
}

}  // namespace blink
