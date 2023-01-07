// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/font_test_base.h"

#include "third_party/blink/renderer/platform/fonts/font_global_context.h"

namespace blink {

FontTestBase::FontTestBase() {
  FontGlobalContext::Init();
}

FontTestBase::~FontTestBase() = default;

}  // namespace blink
