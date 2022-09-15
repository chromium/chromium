// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_MOJOM_TRAITS_H_

#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/transferable_socket.h"
#include "services/network/public/mojom/transferable_socket.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::TransferableSocketDataView,
                    network::TransferableSocket> {
#if BUILDFLAG(IS_WIN)
  static const std::vector<uint8_t>& protocol_info(
      const network::TransferableSocket& value);
#else
  static mojo::PlatformHandle socket(network::TransferableSocket& value);
#endif  // BUILDFLAG(IS_WIN)
  static bool Read(network::mojom::TransferableSocketDataView in,
                   network::TransferableSocket* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_SOCKET_MOJOM_TRAITS_H_
