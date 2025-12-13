// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SOCKADDR_UTIL_POSIX_H_
#define NET_BASE_SOCKADDR_UTIL_POSIX_H_

#include <string_view>

#include "net/base/net_export.h"

namespace net {

struct SockaddrStorage;

// Fills |address| with |socket_path| and its length. For Android or Linux
// platform, this supports abstract namespaces.
NET_EXPORT bool FillUnixAddress(std::string_view socket_path,
                                bool use_abstract_namespace,
                                SockaddrStorage* address);

}  // namespace net

#endif  // NET_BASE_SOCKADDR_UTIL_POSIX_H_
