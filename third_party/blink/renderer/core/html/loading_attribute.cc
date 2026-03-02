// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/loading_attribute.h"

namespace blink {

LoadingAttributeValue GetLoadingAttributeValue(const String& value) {
  if (EqualIgnoringAsciiCase(value, "lazy")) {
    return LoadingAttributeValue::kLazy;
  }
  if (EqualIgnoringAsciiCase(value, "eager")) {
    return LoadingAttributeValue::kEager;
  }
  return LoadingAttributeValue::kAuto;
}

}  // namespace blink
