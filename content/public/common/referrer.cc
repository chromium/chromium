// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer.h"

#include <atomic>
#include <string>

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/enum_utils.h"
#include "net/base/features.h"
#include "net/url_request/url_request_job.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace content {

Referrer::Referrer(const blink::mojom::Referrer& referrer)
    : url(referrer.url), policy(referrer.policy) {}

// static
Referrer Referrer::SanitizeForRequest(const GURL& request,
                                      const Referrer& referrer) {
  blink::mojom::ReferrerPtr sanitized_referrer = SanitizeForRequest(
      request, blink::mojom::Referrer(referrer.url, referrer.policy));
  return Referrer(sanitized_referrer->url, sanitized_referrer->policy);
}

// static
blink::mojom::ReferrerPtr Referrer::SanitizeForRequest(
    const GURL& request,
    const blink::mojom::Referrer& referrer) {
  network::mojom::ReferrerPolicy effective_policy = referrer.policy;
  if (effective_policy == network::mojom::ReferrerPolicy::kDefault) {
    effective_policy = blink::ReferrerUtils::NetToMojoReferrerPolicy(
        blink::ReferrerUtils::GetDefaultNetReferrerPolicy());
  }
  DCHECK_NE(effective_policy, network::mojom::ReferrerPolicy::kDefault);

  return blink::mojom::Referrer::New(
      net::URLRequestJob::ComputeReferrerForPolicy(
          ReferrerPolicyForUrlRequest(effective_policy),
          referrer.url /* original_referrer */, request /* destination */),
      effective_policy);
}

// static
url::Origin Referrer::SanitizeOriginForRequest(
    const GURL& request,
    const url::Origin& initiator,
    network::mojom::ReferrerPolicy policy) {
  Referrer fake_referrer(initiator.GetURL(), policy);
  Referrer sanitized_referrer = SanitizeForRequest(request, fake_referrer);
  return url::Origin::Create(sanitized_referrer.url);
}

// static
net::ReferrerPolicy Referrer::ReferrerPolicyForUrlRequest(
    network::mojom::ReferrerPolicy referrer_policy) {
  if (referrer_policy == network::mojom::ReferrerPolicy::kDefault) {
    return blink::ReferrerUtils::GetDefaultNetReferrerPolicy();
  }
  return network::ReferrerPolicyForUrlRequest(referrer_policy);
}

// static
network::mojom::ReferrerPolicy Referrer::ConvertToPolicy(int32_t policy) {
  return mojo::ConvertIntToMojoEnum<network::mojom::ReferrerPolicy>(policy)
      .value_or(network::mojom::ReferrerPolicy::kDefault);
}

}  // namespace content
