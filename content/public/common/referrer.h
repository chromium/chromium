// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_REFERRER_H_
#define CONTENT_PUBLIC_COMMON_REFERRER_H_

#include "content/common/content_export.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// This struct holds a referrer URL, as well as the referrer policy to be
// applied to this URL. When passing around referrers that will eventually end
// up being used for URL requests, always use this struct.
//
// TODO(leonhsl): Replace this struct everywhere with blink::mojom::Referrer.

struct CONTENT_EXPORT Referrer {
  Referrer(const GURL& url, network::mojom::ReferrerPolicy policy)
      : url(url), policy(policy) {}
  Referrer() : policy(network::mojom::ReferrerPolicy::kDefault) {}
  explicit Referrer(const blink::mojom::Referrer& referrer);

  GURL url;
  network::mojom::ReferrerPolicy policy;

  static Referrer SanitizeForRequest(const GURL& request,
                                     const Referrer& referrer);
  static blink::mojom::ReferrerPtr SanitizeForRequest(
      const GURL& request,
      const blink::mojom::Referrer& referrer);

  // Returns |initiator| origin sanitized by |policy| so that it can be used
  // when requesting |request| URL.
  //
  // Note that the URL-based sanitization (e.g. SanitizeForRequest above) cannot
  // be used for cases where the referrer URL is missing (e.g. about:blank) but
  // the initiator origin still needs to be used (e.g. when calculating the
  // value of the `Origin` header to use in a POST request).
  static url::Origin SanitizeOriginForRequest(
      const GURL& request,
      const url::Origin& initiator,
      network::mojom::ReferrerPolicy policy);

  static net::ReferrerPolicy ReferrerPolicyForUrlRequest(
      network::mojom::ReferrerPolicy referrer_policy);

  // Validates |policy| to make sure it represents one of the valid
  // net::mojom::ReferrerPolicy enum values and returns it.  The relatively safe
  // |kNever| value is returned if |policy| is not a valid value.
  static network::mojom::ReferrerPolicy ConvertToPolicy(int32_t policy);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_REFERRER_H_
