// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"

namespace blink {
namespace cssvalue {

CSSPendingInterpolationValue* CSSPendingInterpolationValue::Create(Type type) {
  return CssValuePool().PendingInterpolationValue(type);
}

CSSPendingInterpolationValue::CSSPendingInterpolationValue(Type type)
    : CSSValue(kPendingInterpolationClass), type_(type) {}

}  // namespace cssvalue
}  // namespace blink
