// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/mojom/connection_allowlist.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST)
    StructTraits<network::mojom::ConnectionAllowlistDataView,
                 network::ConnectionAllowlist> {
  static const std::vector<std::string>& allowlist(
      const network::ConnectionAllowlist& allowlist) {
    return allowlist.allowlist;
  }

  static const std::optional<std::string>& reporting_endpoint(
      const network::ConnectionAllowlist& allowlist) {
    return allowlist.reporting_endpoint;
  }

  static const std::vector<network::mojom::ConnectionAllowlistIssue>& issues(
      const network::ConnectionAllowlist& allowlist) {
    return allowlist.issues;
  }

  static bool Read(network::mojom::ConnectionAllowlistDataView data,
                   network::ConnectionAllowlist* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST)
    StructTraits<network::mojom::ConnectionAllowlistsDataView,
                 network::ConnectionAllowlists> {
  static const std::optional<network::ConnectionAllowlist>& enforced(
      const network::ConnectionAllowlists& allowlists) {
    return allowlists.enforced;
  }

  static const std::optional<network::ConnectionAllowlist>& report_only(
      const network::ConnectionAllowlists& allowlists) {
    return allowlists.report_only;
  }

  static bool Read(network::mojom::ConnectionAllowlistsDataView data,
                   network::ConnectionAllowlists* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_MOJOM_TRAITS_H_
