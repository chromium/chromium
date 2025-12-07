// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_URL_COMPUTATIONS_H_
#define CONTENT_BROWSER_WEBID_URL_COMPUTATIONS_H_

#include <optional>
#include <string>
#include <vector>

#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"

namespace content {
namespace webid {

// This file contains functions that compute URLs that are used in FedCM.

// Computes the URL-encoded POST data for the token endpoint.
std::string ComputeUrlEncodedTokenPostData(
    RenderFrameHost& render_frame_host,
    const std::string& client_id,
    const std::string& nonce,
    const std::string& account_id,
    bool is_auto_reauthn,
    const blink::mojom::RpMode& rp_mode,
    const std::optional<std::vector<std::string>>& fields,
    const std::vector<std::string>& disclosure_shown_for,
    const std::string& params_json,
    const std::optional<std::string>& type);

struct IdentityProviderLoginUrlInfo {
  std::string login_hint;
  std::string domain_hint;
};

void MaybeAppendQueryParameters(
    const IdentityProviderLoginUrlInfo& idp_login_info,
    GURL* login_url);

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_URL_COMPUTATIONS_H_
