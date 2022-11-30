// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/variable.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

bool Variable::IsStaticInstance(const CSSProperty& property) {
  return &property == &GetCSSPropertyVariable();
}

}  // namespace blink
