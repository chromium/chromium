// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_light_dark_color_pair.h"

namespace blink {

String CSSLightDarkColorPair::CustomCSSText() const {
  String first = First().CssText();
  String second = Second().CssText();
  return "-internal-light-dark-color(" + first + ", " + second + ")";
}

}  // namespace blink
