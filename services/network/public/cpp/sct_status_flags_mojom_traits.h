// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SCT_STATUS_FLAGS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SCT_STATUS_FLAGS_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/cert/sct_status_flags.h"
#include "services/network/public/mojom/sct_status_flags.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    EnumTraits<network::mojom::SCTVerifyStatus, net::ct::SCTVerifyStatus> {
  static network::mojom::SCTVerifyStatus ToMojom(
      net::ct::SCTVerifyStatus status);
  static net::ct::SCTVerifyStatus FromMojom(
      network::mojom::SCTVerifyStatus input);
};

}  // namespace mojo
#endif  // SERVICES_NETWORK_PUBLIC_CPP_SCT_STATUS_FLAGS_MOJOM_TRAITS_H_
