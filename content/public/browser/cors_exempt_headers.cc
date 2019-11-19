// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/cors_exempt_headers.h"

#include "content/public/common/content_constants.h"

namespace content {

void UpdateCorsExemptHeader(network::mojom::NetworkContextParams* params) {
  // Note: This mechanism will be deprecated in the near future. You can find
  // a recommended alternative approach on URLRequest::cors_exempt_headers at
  // services/network/public/mojom/url_loader.mojom.
  params->cors_exempt_header_list.push_back(kCorsExemptPurposeHeaderName);
  params->cors_exempt_header_list.push_back(kCorsExemptRequestedWithHeaderName);
}

}  // namespace content
