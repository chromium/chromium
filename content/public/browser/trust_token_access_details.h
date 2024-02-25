// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRUST_TOKEN_ACCESS_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_TRUST_TOKEN_ACCESS_DETAILS_H_

#include <optional>

#include "content/common/content_export.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT TrustTokenAccessDetails {
  TrustTokenAccessDetails();
  TrustTokenAccessDetails(const url::Origin& origin,
                          network::mojom::TrustTokenOperationType type,
                          const std::optional<url::Origin>& issuer,
                          bool blocked);
  explicit TrustTokenAccessDetails(
      const network::mojom::TrustTokenAccessDetailsPtr& details);
  ~TrustTokenAccessDetails();

  TrustTokenAccessDetails(const TrustTokenAccessDetails&);
  TrustTokenAccessDetails& operator=(const TrustTokenAccessDetails&);

  url::Origin origin;
  network::mojom::TrustTokenOperationType type;
  std::optional<url::Origin> issuer;
  bool blocked = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRUST_TOKEN_ACCESS_DETAILS_H_
