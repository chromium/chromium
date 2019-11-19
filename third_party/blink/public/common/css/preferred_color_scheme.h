// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CSS_PREFERRED_COLOR_SCHEME_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CSS_PREFERRED_COLOR_SCHEME_H_

namespace blink {

// Use for passing preferred color scheme from the OS to the renderer.
enum class PreferredColorScheme {
  kNoPreference,
  kDark,
  kLight,
};

}  // namespace blink

#endif
