// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_UTIL_INTERNAL_H_
#define CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_UTIL_INTERNAL_H_

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

// Sets the flag of where the network service is forced to be running in the
// browser process.
CONTENT_EXPORT void ForceInProcessNetworkServiceImpl();
CONTENT_EXPORT void ForceOutOfProcessNetworkServiceImpl();
// Returns true if the network service is enabled and it's running in the
// browser process.
CONTENT_EXPORT bool IsInProcessNetworkServiceImpl();

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_NETWORK_SERVICE_UTIL_INTERNAL_H_
