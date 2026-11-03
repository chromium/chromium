// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_param_value_pair.h"

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

String CSSParamValuePair::CustomCSSText() const {
  return StrCat({"param(", Name().CssText(), ": ", Value().CssText(), ")"});
}

}  // namespace blink
