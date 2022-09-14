// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_INTERFACES_POSIX_H_
#define NET_BASE_NETWORK_INTERFACES_POSIX_H_

// This file provides some basic functionality shared between
// network_interfaces_linux.cc and network_interfaces_getifaddrs.cc.

#include <string>

struct sockaddr;

namespace net::internal {

bool ShouldIgnoreInterface(const std::string& name, int policy);
bool IsLoopbackOrUnspecifiedAddress(const sockaddr* addr);

}  // namespace net::internal

#endif  // NET_BASE_NETWORK_INTERFACES_POSIX_H_
