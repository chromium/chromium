// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_IMPORTANCE_ATTRIBUTE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_IMPORTANCE_ATTRIBUTE_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

mojom::FetchImportanceMode GetFetchImportanceAttributeValue(
    const String& value);

}  // namespace blink

#endif
