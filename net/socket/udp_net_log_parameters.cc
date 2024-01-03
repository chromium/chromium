// Copyright 2012 The Chromium Authors
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

base::Value::Dict NetLogUDPDataTransferParams(int byte_count,
                                              const char* bytes,
                                              const IPEndPoint* address,
                                              NetLogCaptureMode capture_mode) {
  auto dict = base::Value::Dict().Set("byte_count", byte_count);
  if (NetLogCaptureIncludesSocketBytes(capture_mode))
    dict.Set("bytes", NetLogBinaryValue(bytes, byte_count));
  if (address)
    dict.Set("address", address->ToString());
  return dict;
}

base::Value::Dict NetLogUDPConnectParams(const IPEndPoint& address,
                                         handles::NetworkHandle network) {
  auto dict = base::Value::Dict().Set("address", address.ToString());
  if (network != handles::kInvalidNetworkHandle)
    dict.Set("bound_to_network", static_cast<int>(network));
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

base::Value::Dict CreateNetLogUDPConnectParams(const IPEndPoint& address,
                                               handles::NetworkHandle network) {
  return NetLogUDPConnectParams(address, network);
}

}  // namespace net
