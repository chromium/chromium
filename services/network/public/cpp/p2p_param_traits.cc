// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/p2p_param_traits.h"

#include "base/notreached.h"
#include "ipc/param_traits_utils.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"

// Generation of IPC definitions.

// Generate param traits write methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "p2p_param_traits.h"
}  // namespace IPC

// Generate param traits read methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "p2p_param_traits.h"
}  // namespace IPC

namespace mojo {
network::mojom::EcnMarking
EnumTraits<network::mojom::EcnMarking, webrtc::EcnMarking>::ToMojom(
    webrtc::EcnMarking input) {
  switch (input) {
    case webrtc::EcnMarking::kNotEct:
      return network::mojom::EcnMarking::kNotEct;
    case webrtc::EcnMarking::kEct1:
      return network::mojom::EcnMarking::kEct1;
    case webrtc::EcnMarking::kEct0:
      return network::mojom::EcnMarking::kEct0;
    case webrtc::EcnMarking::kCe:
      return network::mojom::EcnMarking::kCe;
  }
  NOTREACHED();
}

webrtc::EcnMarking
EnumTraits<network::mojom::EcnMarking, webrtc::EcnMarking>::FromMojom(
    network::mojom::EcnMarking input) {
  switch (input) {
    case network::mojom::EcnMarking::kNotEct:
      return webrtc::EcnMarking::kNotEct;
    case network::mojom::EcnMarking::kEct1:
      return webrtc::EcnMarking::kEct1;
    case network::mojom::EcnMarking::kEct0:
      return webrtc::EcnMarking::kEct0;
    case network::mojom::EcnMarking::kCe:
      return webrtc::EcnMarking::kCe;
  }
  NOTREACHED();
}
}  // namespace mojo
