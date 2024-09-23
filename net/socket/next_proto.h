// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_NEXT_PROTO_H_
#define NET_SOCKET_NEXT_PROTO_H_

#include <string_view>
#include <vector>

#include "net/base/net_export.h"

namespace net {

// This enum is used in Net.SSLNegotiatedAlpnProtocol histogram.
// Do not change or re-use values.
enum NextProto {
  kProtoUnknown = 0,
  kProtoHTTP11 = 1,
  kProtoHTTP2 = 2,
  kProtoQUIC = 3,
  kProtoLast = kProtoQUIC
};

// List of protocols to use for ALPN, used for configuring HttpNetworkSessions.
typedef std::vector<NextProto> NextProtoVector;

NET_EXPORT_PRIVATE NextProto NextProtoFromString(std::string_view proto_string);

NET_EXPORT_PRIVATE const char* NextProtoToString(NextProto next_proto);

}  // namespace net

#endif  // NET_SOCKET_NEXT_PROTO_H_
