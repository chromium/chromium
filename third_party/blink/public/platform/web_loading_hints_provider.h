// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LOADING_HINTS_PROVIDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LOADING_HINTS_PROVIDER_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

struct WebLoadingHintsProvider {
  WebLoadingHintsProvider(
      int64_t ukm_source_id,
      const blink::WebVector<blink::WebString>& subresource_patterns_to_block)
      : ukm_source_id(ukm_source_id),
        subresource_patterns_to_block(subresource_patterns_to_block) {}

  const int64_t ukm_source_id;
  const blink::WebVector<blink::WebString> subresource_patterns_to_block;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LOADING_HINTS_PROVIDER_H_
