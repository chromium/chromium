// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/line_clamp_data.h"

#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsLineClampData {
  LayoutUnit clamp_bfc_offset;
  int lines_until_clamp;
  int state;
};

ASSERT_SIZE(LineClampData, SameSizeAsLineClampData);

}  // namespace

}  // namespace blink
