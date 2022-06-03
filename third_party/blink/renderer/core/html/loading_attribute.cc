// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/loading_attribute.h"

namespace blink {

LoadingAttributeValue GetLoadingAttributeValue(const String& value) {
  if (EqualIgnoringASCIICase(value, "lazy"))
    return LoadingAttributeValue::kLazy;
  if (EqualIgnoringASCIICase(value, "eager"))
    return LoadingAttributeValue::kEager;
  return LoadingAttributeValue::kAuto;
}

}  // namespace blink
