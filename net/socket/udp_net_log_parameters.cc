// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/udp_net_log_parameters.h"

#include <utility>

#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"

namespace net {

namespace {

base::Value NetLogUDPDataTransferParams(int byte_count,
                                        const char* bytes,
                                        const IPEndPoint* address,
                                        NetLogCaptureMode capture_mode) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("byte_count", byte_count);
  if (NetLogCaptureIncludesSocketBytes(capture_mode))
    dict.SetKey("bytes", NetLogBinaryValue(bytes, byte_count));
  if (address)
    dict.SetStringKey("address", address->ToString());
  return dict;
}

base::Value NetLogUDPConnectParams(
    const IPEndPoint& address,
    NetworkChangeNotifier::NetworkHandle network) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("address", address.ToString());
  if (network != NetworkChangeNotifier::kInvalidNetworkHandle)
    dict.SetIntKey("bound_to_network", network);
  return dict;
}

}  // namespace

void NetLogUDPDataTransfer(const NetLogWithSource& net_log,
                           NetLogEventType type,
                           int byte_count,
                           const char* bytes,
                           const IPEndPoint* address) {
  DCHECK(bytes);
  net_log.AddEvent(type, [&](NetLogCaptureMode capture_mode) {
    return NetLogUDPDataTransferParams(byte_count, bytes, address,
                                       capture_mode);
  });
}

base::Value CreateNetLogUDPConnectParams(
    const IPEndPoint& address,
    NetworkChangeNotifier::NetworkHandle network) {
  return NetLogUDPConnectParams(address, network);
}

}  // namespace net
