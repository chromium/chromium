// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DEVTOOLS_OBSERVER_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DEVTOOLS_OBSERVER_UTIL_H_

#include "base/component_export.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"

namespace network {
struct ResourceRequest;

namespace mojom {
class URLResponseHead;
}

COMPONENT_EXPORT(NETWORK_CPP)
mojom::URLResponseHeadDevToolsInfoPtr ExtractDevToolsInfo(
    const mojom::URLResponseHead& head);
COMPONENT_EXPORT(NETWORK_CPP)
mojom::URLRequestDevToolsInfoPtr ExtractDevToolsInfo(
    const ResourceRequest& request);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DEVTOOLS_OBSERVER_UTIL_H_
