// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_APPLE_BUILDFLAGS_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_APPLE_BUILDFLAGS_H_

#include <Availability.h>

#include "build/build_config.h"

// When defined, NetworkChangeNotifier uses the legacy SCNetworkReachability API
// for network change notifications. When this is not defined, the notifier
// uses the newer NetworkPathMonitor API.
#if BUILDFLAG(IS_MAC) || !defined(__IPHONE_17_4) || \
    __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_4
#define COMPILE_OLD_NOTIFIER_IMPL 1
#endif  // BUILDFLAG(IS_MAC) || !defined(__IPHONE_17_4) ||
        // __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_4

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_APPLE_BUILDFLAGS_H_
