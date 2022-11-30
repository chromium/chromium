// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_NETWORK_SERVICE_UTIL_H_
#define CONTENT_PUBLIC_COMMON_NETWORK_SERVICE_UTIL_H_

#include "content/common/content_export.h"

namespace content {

// Returns true if the network service is enabled and it's running in a separate
// process.
CONTENT_EXPORT bool IsOutOfProcessNetworkService();

// Returns true if the network service is enabled and it's running in the
// browser process.
CONTENT_EXPORT bool IsInProcessNetworkService();

// Sets the flag of whether the network service is forced to be running in the
// browser process. The flag will be checked in |IsInProcessNetworkService()|.
CONTENT_EXPORT void ForceInProcessNetworkService(bool is_forced);
}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_NETWORK_SERVICE_UTIL_H_
