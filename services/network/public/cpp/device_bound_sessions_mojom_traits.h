// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_

#include "net/device_bound_sessions/session_key.h"
#include "services/network/public/mojom/device_bound_sessions.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::DeviceBoundSessionKeyDataView,
                    net::device_bound_sessions::SessionKey> {
  static const net::SchemefulSite& site(
      const net::device_bound_sessions::SessionKey& obj) {
    return obj.site;
  }

  static const std::string& id(
      const net::device_bound_sessions::SessionKey& obj) {
    return obj.id.value();
  }

  static bool Read(network::mojom::DeviceBoundSessionKeyDataView data,
                   net::device_bound_sessions::SessionKey* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_
