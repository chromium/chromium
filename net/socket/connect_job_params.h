// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CONNECT_JOB_PARAMS_H_
#define NET_SOCKET_CONNECT_JOB_PARAMS_H_

#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace net {

class HttpProxySocketParams;
class SOCKSSocketParams;
class TransportSocketParams;
class SSLSocketParams;

// Abstraction over the param types for various connect jobs.
class NET_EXPORT_PRIVATE ConnectJobParams {
 public:
  ConnectJobParams();
  explicit ConnectJobParams(scoped_refptr<HttpProxySocketParams> params);
  explicit ConnectJobParams(scoped_refptr<SOCKSSocketParams> params);
  explicit ConnectJobParams(scoped_refptr<TransportSocketParams> params);
  explicit ConnectJobParams(scoped_refptr<SSLSocketParams> params);
  ~ConnectJobParams();

  ConnectJobParams(ConnectJobParams&);
  ConnectJobParams& operator=(ConnectJobParams&);
  ConnectJobParams(ConnectJobParams&&);
  ConnectJobParams& operator=(ConnectJobParams&&);

  bool is_http_proxy() const {
    return absl::holds_alternative<scoped_refptr<HttpProxySocketParams>>(
        params_);
  }

  bool is_socks() const {
    return absl::holds_alternative<scoped_refptr<SOCKSSocketParams>>(params_);
  }

  bool is_transport() const {
    return absl::holds_alternative<scoped_refptr<TransportSocketParams>>(
        params_);
  }

  bool is_ssl() const {
    return absl::holds_alternative<scoped_refptr<SSLSocketParams>>(params_);
  }

  // Get lvalue references to the contained params.
  const scoped_refptr<HttpProxySocketParams>& http_proxy() const {
    return get<scoped_refptr<HttpProxySocketParams>>(params_);
  }
  const scoped_refptr<SOCKSSocketParams>& socks() const {
    return get<scoped_refptr<SOCKSSocketParams>>(params_);
  }
  const scoped_refptr<TransportSocketParams>& transport() const {
    return get<scoped_refptr<TransportSocketParams>>(params_);
  }
  const scoped_refptr<SSLSocketParams>& ssl() const {
    return get<scoped_refptr<SSLSocketParams>>(params_);
  }

  // Take params out of this value.
  scoped_refptr<HttpProxySocketParams>&& take_http_proxy() {
    return get<scoped_refptr<HttpProxySocketParams>>(std::move(params_));
  }
  scoped_refptr<SOCKSSocketParams>&& take_socks() {
    return get<scoped_refptr<SOCKSSocketParams>>(std::move(params_));
  }
  scoped_refptr<TransportSocketParams>&& take_transport() {
    return get<scoped_refptr<TransportSocketParams>>(std::move(params_));
  }
  scoped_refptr<SSLSocketParams>&& take_ssl() {
    return get<scoped_refptr<SSLSocketParams>>(std::move(params_));
  }

 private:
  absl::variant<scoped_refptr<HttpProxySocketParams>,
                scoped_refptr<SOCKSSocketParams>,
                scoped_refptr<TransportSocketParams>,
                scoped_refptr<SSLSocketParams>>
      params_;
};

}  // namespace net

#endif  // NET_SOCKET_CONNECT_JOB_PARAMS_H_
