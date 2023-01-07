// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/fetch_priority_attribute.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"

namespace blink {

mojom::blink::FetchPriorityHint GetFetchPriorityAttributeValue(
    const String& value) {
  if (EqualIgnoringASCIICase(value, "low"))
    return mojom::blink::FetchPriorityHint::kLow;
  if (EqualIgnoringASCIICase(value, "high"))
    return mojom::blink::FetchPriorityHint::kHigh;
  return mojom::blink::FetchPriorityHint::kAuto;
}

}  // namespace blink
