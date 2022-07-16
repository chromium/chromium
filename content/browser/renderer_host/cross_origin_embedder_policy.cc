// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/renderer_host/cross_origin_embedder_policy.h"

#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace content {

network::CrossOriginEmbedderPolicy CoepFromMainResponse(
    const GURL& context_url,
    const network::mojom::URLResponseHead* context_main_response) {
  network::CrossOriginEmbedderPolicy coep =
      context_main_response->parsed_headers->cross_origin_embedder_policy;

  if (base::FeatureList::IsEnabled(
          network::features::kCrossOriginEmbedderPolicyCredentialless)) {
    return coep;
  }

  // At this point COEP:credentialless is not enabled on this context. So it
  // needs to be cleared.

  using CoepValue = network::mojom::CrossOriginEmbedderPolicyValue;
  if (coep.value == CoepValue::kCredentialless) {
    coep.value = CoepValue::kNone;
    coep.reporting_endpoint.reset();
  }

  if (coep.report_only_value == CoepValue::kCredentialless) {
    coep.report_only_value = CoepValue::kNone;
    coep.report_only_reporting_endpoint.reset();
  }

  return coep;
}

}  // namespace content
