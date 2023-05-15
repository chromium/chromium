// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_EMPTY_NETWORK_SERVICE_H_
#define SERVICES_NETWORK_EMPTY_NETWORK_SERVICE_H_

#include "base/component_export.h"

namespace mojo {
class ServiceFactory;
}  // namespace mojo

namespace network {
COMPONENT_EXPORT(NETWORK_SERVICE)
void RegisterEmptyNetworkService(mojo::ServiceFactory& services);
}  // namespace network

#endif  // SERVICES_NETWORK_EMPTY_NETWORK_SERVICE_H_
