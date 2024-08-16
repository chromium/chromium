// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/p2p_param_traits.h"

#include "ipc/ipc_message_utils.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"

// Generation of IPC definitions.

// Generate constructors.
#undef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#include "ipc/struct_constructor_macros.h"
#include "p2p_param_traits.h"

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

// Generate param traits log methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "p2p_param_traits.h"
}  // namespace IPC

namespace mojo {
network::mojom::EcnMarking
EnumTraits<network::mojom::EcnMarking, rtc::EcnMarking>::ToMojom(
    rtc::EcnMarking input) {
  switch (input) {
    case rtc::EcnMarking::kNotEct:
      return network::mojom::EcnMarking::kNotEct;
    case rtc::EcnMarking::kEct1:
      return network::mojom::EcnMarking::kEct1;
    case rtc::EcnMarking::kEct0:
      return network::mojom::EcnMarking::kEct0;
    case rtc::EcnMarking::kCe:
      return network::mojom::EcnMarking::kCe;
  }
  NOTREACHED();
}

bool EnumTraits<network::mojom::EcnMarking, rtc::EcnMarking>::FromMojom(
    network::mojom::EcnMarking input,
    rtc::EcnMarking* output) {
  switch (input) {
    case network::mojom::EcnMarking::kNotEct:
      *output = rtc::EcnMarking::kNotEct;
      return true;
    case network::mojom::EcnMarking::kEct1:
      *output = rtc::EcnMarking::kEct1;
      return true;
    case network::mojom::EcnMarking::kEct0:
      *output = rtc::EcnMarking::kEct0;
      return true;
    case network::mojom::EcnMarking::kCe:
      *output = rtc::EcnMarking::kCe;
      return true;
  }
  return false;
}
}  // namespace mojo
