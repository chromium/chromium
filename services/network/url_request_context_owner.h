// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_
#define SERVICES_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "components/ip_protection/common/ip_protection_control_mojo.h"

class PrefService;

namespace net {
class URLRequestContext;
}

namespace network {

// This owns a net::URLRequestContext and other state that's used with it.
struct COMPONENT_EXPORT(NETWORK_SERVICE) URLRequestContextOwner {
  URLRequestContextOwner();
  URLRequestContextOwner(
      std::unique_ptr<PrefService> pref_service,
      std::unique_ptr<net::URLRequestContext> url_request_context,
      std::unique_ptr<ip_protection::IpProtectionControlMojo>
          ip_protection_control_mojo);
  ~URLRequestContextOwner();
  URLRequestContextOwner(URLRequestContextOwner&& other);
  URLRequestContextOwner& operator=(URLRequestContextOwner&& other);

  // This needs to be destroyed after the URLRequestContext.
  std::unique_ptr<PrefService> pref_service;

  std::unique_ptr<net::URLRequestContext> url_request_context;

  // `IpProtectionControlMojo` calls into `IpProtectionCoreImpl` so we need to
  // ensure it is destroyed before `IpProtectionCoreImpl` is destroyed. The
  // lifetime of `IpProtectionCoreImpl` is tied to the lifetime of
  // `IpProtectionProxyDelegate` which itself is tied to the lifetime of
  // `URLRequestContext`, so we just need to ensure `IpProtectionControlMojo`
  // is destroyed before `URLRequestContext` is destroyed.
  std::unique_ptr<ip_protection::IpProtectionControlMojo>
      ip_protection_control_mojo;
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_REQUEST_CONTEXT_OWNER_H_
