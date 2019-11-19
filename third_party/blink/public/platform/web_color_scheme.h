// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COLOR_SCHEME_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COLOR_SCHEME_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This is the color scheme for rendering web content. The color scheme affects
// the UA style sheet (setting the text color to white instead of black on the
// root element for kDark), the frame backdrop color (black instead of white for
// kDark), theming form controls and scrollbars, etc.
enum WebColorScheme {
  kLight = 1,
  kDark = 2,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COLOR_SCHEME_H_
