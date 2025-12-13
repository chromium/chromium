// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/identity_provider_info.h"

namespace content::webid {

IdentityProviderInfo::IdentityProviderInfo(
    const blink::mojom::IdentityProviderRequestOptionsPtr& provider,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata metadata,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    std::optional<blink::mojom::Format> format)
    : provider(provider->Clone()),
      endpoints(std::move(endpoints)),
      metadata(std::move(metadata)),
      rp_context(rp_context),
      rp_mode(rp_mode),
      format(format) {}

IdentityProviderInfo::~IdentityProviderInfo() = default;
IdentityProviderInfo::IdentityProviderInfo(const IdentityProviderInfo& other) {
  provider = other.provider->Clone();
  endpoints = other.endpoints;
  metadata = other.metadata;
  has_failing_idp_signin_status = other.has_failing_idp_signin_status;
  rp_context = other.rp_context;
  rp_mode = other.rp_mode;
  data = other.data;
  format = other.format;
}

}  // namespace content::webid
