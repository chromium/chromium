// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/importance_attribute.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"

namespace blink {

mojom::FetchImportanceMode GetFetchImportanceAttributeValue(
    const String& value) {
  if (EqualIgnoringASCIICase(value, "low"))
    return mojom::FetchImportanceMode::kImportanceLow;
  if (EqualIgnoringASCIICase(value, "high"))
    return mojom::FetchImportanceMode::kImportanceHigh;
  return mojom::FetchImportanceMode::kImportanceAuto;
}

}  // namespace blink
