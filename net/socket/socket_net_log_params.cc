// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_net_log_params.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_with_source.h"

namespace net {

base::Value::Dict NetLogSocketErrorParams(int net_error, int os_error) {
  base::Value::Dict dict;
  dict.Set("net_error", net_error);
  dict.Set("os_error", os_error);
  return dict;
}

void NetLogSocketError(const NetLogWithSource& net_log,
                       NetLogEventType type,
                       int net_error,
                       int os_error) {
  net_log.AddEvent(
      type, [&] { return NetLogSocketErrorParams(net_error, os_error); });
}

base::Value::Dict CreateNetLogHostPortPairParams(
    const HostPortPair* host_and_port) {
  base::Value::Dict dict;
  dict.Set("host_and_port", host_and_port->ToString());
  return dict;
}

base::Value::Dict CreateNetLogIPEndPointParams(const IPEndPoint* address) {
  base::Value::Dict dict;
  dict.Set("address", address->ToString());
  return dict;
}

base::Value::Dict CreateNetLogAddressPairParams(
    const net::IPEndPoint& local_address,
    const net::IPEndPoint& remote_address) {
  base::Value::Dict dict;
  dict.Set("local_address", local_address.ToString());
  dict.Set("remote_address", remote_address.ToString());
  return dict;
}

}  // namespace net
