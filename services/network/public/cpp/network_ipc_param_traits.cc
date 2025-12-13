// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_ipc_param_traits.h"

#include "ipc/mojo_param_traits.h"
#include "ipc/param_traits_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_util.h"

// Generation of IPC definitions.

// Generate param traits write methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "network_ipc_param_traits.h"
}  // namespace IPC

// Generate param traits read methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "network_ipc_param_traits.h"
}  // namespace IPC
