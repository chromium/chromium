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
