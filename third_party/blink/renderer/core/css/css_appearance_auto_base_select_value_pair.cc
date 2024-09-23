// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_appearance_auto_base_select_value_pair.h"

namespace blink {

String CSSAppearanceAutoBaseSelectValuePair::CustomCSSText() const {
  String first = First().CssText();
  String second = Second().CssText();
  return "-internal-appearance-auto-base-select(" + first + ", " + second + ")";
}

}  // namespace blink
