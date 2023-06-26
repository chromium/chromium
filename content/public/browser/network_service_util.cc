// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/network_service_util.h"

#include "content/browser/network/network_service_util_internal.h"

namespace content {
bool IsOutOfProcessNetworkService() {
  return !IsInProcessNetworkService();
}

bool IsInProcessNetworkService() {
  return IsInProcessNetworkServiceImpl();
}

void ForceOutOfProcessNetworkService() {
  ForceOutOfProcessNetworkServiceImpl();
}
void ForceInProcessNetworkService() {
  ForceInProcessNetworkServiceImpl();
}

}  // namespace content
