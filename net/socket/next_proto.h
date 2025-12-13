// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_NEXT_PROTO_H_
#define NET_SOCKET_NEXT_PROTO_H_

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/containers/enum_set.h"
#include "net/base/net_export.h"

namespace net {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NextProto)
enum class NextProto : uint8_t {
  kProtoUnknown = 0,
  kProtoHTTP11 = 1,
  kProtoHTTP2 = 2,
  kProtoQUIC = 3,
  kMaxValue = kProtoQUIC,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SSLNegotiatedAlpnProtocol)

// List of protocols to use for ALPN, used for configuring HttpNetworkSessions.
typedef std::vector<NextProto> NextProtoVector;

using NextProtoSet =
    base::EnumSet<NextProto, NextProto::kProtoUnknown, NextProto::kMaxValue>;

NET_EXPORT_PRIVATE NextProto NextProtoFromString(std::string_view proto_string);

NET_EXPORT_PRIVATE const char* NextProtoToString(NextProto next_proto);

// Used for histograms.
NET_EXPORT_PRIVATE const std::string_view NegotiatedProtocolToHistogramSuffix(
    NextProto next_proto);

// Similar to NegotiatedProtocolToHistogramSuffix, but `kProtoUnknown` is
// treated as HTTP/1.1.
NET_EXPORT_PRIVATE const std::string_view
NegotiatedProtocolToHistogramSuffixCoalesced(NextProto next_proto);

}  // namespace net

#endif  // NET_SOCKET_NEXT_PROTO_H_
