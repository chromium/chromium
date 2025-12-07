// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/device_bound_sessions_mojom_traits.h"

#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/device_bound_sessions/session_params.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "url/gurl.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::DeviceBoundSessionKeyDataView,
                  net::device_bound_sessions::SessionKey>::
    Read(network::mojom::DeviceBoundSessionKeyDataView data,
         net::device_bound_sessions::SessionKey* out) {
  if (!data.ReadSite(&out->site)) {
    return false;
  }

  if (!data.ReadId(&out->id.value())) {
    return false;
  }

  return true;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionAccessDataView,
                  net::device_bound_sessions::SessionAccess>::
    Read(network::mojom::DeviceBoundSessionAccessDataView data,
         net::device_bound_sessions::SessionAccess* out) {
  if (!data.ReadAccessType(&out->access_type)) {
    return false;
  }

  if (!data.ReadSessionKey(&out->session_key)) {
    return false;
  }

  if (!data.ReadCookies(&out->cookies)) {
    return false;
  }

  return true;
}

// static
bool StructTraits<
    network::mojom::DeviceBoundSessionScopeSpecificationDataView,
    net::device_bound_sessions::SessionParams::Scope::Specification>::
    Read(network::mojom::DeviceBoundSessionScopeSpecificationDataView data,
         net::device_bound_sessions::SessionParams::Scope::Specification* out) {
  if (!data.ReadType(&out->type)) {
    return false;
  }
  if (!data.ReadDomain(&out->domain)) {
    return false;
  }
  if (!data.ReadPath(&out->path)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionScopeDataView,
                  net::device_bound_sessions::SessionParams::Scope>::
    Read(network::mojom::DeviceBoundSessionScopeDataView data,
         net::device_bound_sessions::SessionParams::Scope* out) {
  out->include_site = data.include_site();
  if (!data.ReadSpecifications(&out->specifications)) {
    return false;
  }
  if (!data.ReadOrigin(&out->origin)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionCredentialDataView,
                  net::device_bound_sessions::SessionParams::Credential>::
    Read(network::mojom::DeviceBoundSessionCredentialDataView data,
         net::device_bound_sessions::SessionParams::Credential* out) {
  if (!data.ReadName(&out->name)) {
    return false;
  }
  if (!data.ReadAttributes(&out->attributes)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::DeviceBoundSessionParamsDataView,
                  net::device_bound_sessions::SessionParams>::
    Read(network::mojom::DeviceBoundSessionParamsDataView data,
         net::device_bound_sessions::SessionParams* out) {
  std::string session_id;
  GURL fetcher_url;
  std::string refresh_url;
  net::device_bound_sessions::SessionParams::Scope scope;
  std::vector<net::device_bound_sessions::SessionParams::Credential>
      credentials;
  std::vector<std::string> allowed_refresh_initiators;

  if (!data.ReadSessionId(&session_id) || !data.ReadFetcherUrl(&fetcher_url) ||
      !data.ReadRefreshUrl(&refresh_url) || !data.ReadScope(&scope) ||
      !data.ReadCredentials(&credentials) ||
      !data.ReadAllowedRefreshInitiators(&allowed_refresh_initiators)) {
    return false;
  }

  *out = net::device_bound_sessions::SessionParams(
      std::move(session_id), std::move(fetcher_url), std::move(refresh_url),
      std::move(scope), std::move(credentials),
      unexportable_keys::UnexportableKeyId(),
      std::move(allowed_refresh_initiators));

  return true;
}

}  // namespace mojo
