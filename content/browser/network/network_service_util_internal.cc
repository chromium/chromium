// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_service_util_internal.h"

#include "base/check.h"

namespace {
absl::optional<bool> g_force_network_service_process_in_or_out;

}  // namespace

namespace content {

void ForceInProcessNetworkServiceImpl() {
  DCHECK(!g_force_network_service_process_in_or_out ||
         *g_force_network_service_process_in_or_out);
  g_force_network_service_process_in_or_out = true;
}
void ForceOutOfProcessNetworkServiceImpl() {
  DCHECK(!g_force_network_service_process_in_or_out ||
         !*g_force_network_service_process_in_or_out);
  g_force_network_service_process_in_or_out = false;
}

absl::optional<bool> GetForcedNetworkServiceProcessInOrOut() {
  return g_force_network_service_process_in_or_out;
}

}  // namespace content
