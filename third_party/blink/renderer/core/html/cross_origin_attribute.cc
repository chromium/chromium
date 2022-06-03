// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"

namespace blink {

CrossOriginAttributeValue GetCrossOriginAttributeValue(const String& value) {
  if (value.IsNull())
    return kCrossOriginAttributeNotSet;
  if (EqualIgnoringASCIICase(value, "use-credentials"))
    return kCrossOriginAttributeUseCredentials;
  return kCrossOriginAttributeAnonymous;
}

}  // namespace blink
