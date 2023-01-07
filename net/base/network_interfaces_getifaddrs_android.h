// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_INTERFACES_GETIFADDRS_ANDROID_H_
#define NET_BASE_NETWORK_INTERFACES_GETIFADDRS_ANDROID_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

#include <ifaddrs.h>

namespace net::internal {

// Implementation of getifaddrs for Android.
// Fills out a list of ifaddr structs (see below) which contain information
// about every network interface available on the host.
// See 'man getifaddrs' on Linux or OS X (nb: it is not a POSIX function).
// Due to some buggy getifaddrs() implementation in Android 11, Chromium
// provides its own version. See https://crbug.com/1240237 for more context.
// ifa_ifu(ifa_broadaddr, ifa_dstaddr) is not populated in this function.
int Getifaddrs(struct ifaddrs** result);
void Freeifaddrs(struct ifaddrs* addrs);

}  // namespace net::internal

#endif  // BUILDFLAG(IS_ANDROID)

#endif  // NET_BASE_NETWORK_INTERFACES_GETIFADDRS_ANDROID_H_
