// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PARSED_REQUEST_COOKIE_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PARSED_REQUEST_COOKIE_MOJOM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/parsed_request_cookie.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::ParsedRequestCookieDataView,
                 net::cookie_util::ParsedRequestCookie> {
 public:
  static const std::string& name(
      const net::cookie_util::ParsedRequestCookie& cookie) {
    return cookie.first;
  }

  static const std::string& value(
      const net::cookie_util::ParsedRequestCookie& cookie) {
    return cookie.second;
  }

  static bool Read(network::mojom::ParsedRequestCookieDataView data,
                   net::cookie_util::ParsedRequestCookie* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PARSED_REQUEST_COOKIE_MOJOM_TRAITS_H_
