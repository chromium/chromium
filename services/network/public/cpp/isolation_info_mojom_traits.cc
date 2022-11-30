// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/isolation_info_mojom_traits.h"

#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/cookie_manager_shared_mojom_traits.h"
#include "services/network/public/cpp/crash_keys.h"

namespace mojo {

bool EnumTraits<network::mojom::IsolationInfoRequestType,
                net::IsolationInfo::RequestType>::
    FromMojom(network::mojom::IsolationInfoRequestType request_type,
              net::IsolationInfo::RequestType* out) {
  switch (request_type) {
    case network::mojom::IsolationInfoRequestType::kMainFrame:
      *out = net::IsolationInfo::RequestType::kMainFrame;
      return true;
    case network::mojom::IsolationInfoRequestType::kSubFrame:
      *out = net::IsolationInfo::RequestType::kSubFrame;
      return true;
    case network::mojom::IsolationInfoRequestType::kOther:
      *out = net::IsolationInfo::RequestType::kOther;
      return true;
  }
  return false;
}

network::mojom::IsolationInfoRequestType EnumTraits<
    network::mojom::IsolationInfoRequestType,
    net::IsolationInfo::RequestType>::ToMojom(net::IsolationInfo::RequestType
                                                  request_type) {
  switch (request_type) {
    case net::IsolationInfo::RequestType::kMainFrame:
      return network::mojom::IsolationInfoRequestType::kMainFrame;
    case net::IsolationInfo::RequestType::kSubFrame:
      return network::mojom::IsolationInfoRequestType::kSubFrame;
    case net::IsolationInfo::RequestType::kOther:
      return network::mojom::IsolationInfoRequestType::kOther;
  }

  NOTREACHED();
  return network::mojom::IsolationInfoRequestType::kOther;
}

bool StructTraits<network::mojom::IsolationInfoDataView, net::IsolationInfo>::
    Read(network::mojom::IsolationInfoDataView data, net::IsolationInfo* out) {
  absl::optional<url::Origin> top_frame_origin;
  absl::optional<url::Origin> frame_origin;
  absl::optional<base::UnguessableToken> nonce;
  net::SiteForCookies site_for_cookies;
  net::IsolationInfo::RequestType request_type;
  absl::optional<std::vector<net::SchemefulSite>> mojo_party_context;

  if (!data.ReadTopFrameOrigin(&top_frame_origin)) {
    network::debug::SetDeserializationCrashKeyString("isolation_top_origin");
    return false;
  }
  if (!data.ReadFrameOrigin(&frame_origin)) {
    network::debug::SetDeserializationCrashKeyString("isolation_frame_origin");
    return false;
  }
  if (!data.ReadNonce(&nonce) || !data.ReadSiteForCookies(&site_for_cookies) ||
      !data.ReadRequestType(&request_type) ||
      !data.ReadPartyContext(&mojo_party_context)) {
    return false;
  }

  absl::optional<std::set<net::SchemefulSite>> party_context;
  if (mojo_party_context.has_value()) {
    party_context = std::set<net::SchemefulSite>(mojo_party_context->begin(),
                                                 mojo_party_context->end());
    if (party_context->size() != mojo_party_context->size())
      return false;
  }

  absl::optional<net::IsolationInfo> isolation_info =
      net::IsolationInfo::CreateIfConsistent(
          request_type, top_frame_origin, frame_origin, site_for_cookies,
          std::move(party_context),
          nonce.has_value() ? &nonce.value() : nullptr);
  if (!isolation_info) {
    network::debug::SetDeserializationCrashKeyString("isolation_inconsistent");
    return false;
  }

  *out = std::move(*isolation_info);
  return true;
}

}  // namespace mojo
