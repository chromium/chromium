// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/mac/core_text_font_format_support.h"

#include "base/mac/mac_util.h"

namespace blink {

bool CoreTextVersionSupportsVariations() {
  return base::mac::IsAtLeastOS10_14();
}

}  // namespace blink
