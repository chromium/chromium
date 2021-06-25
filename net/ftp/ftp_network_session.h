// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_SESSION_H_
#define NET_FTP_FTP_NETWORK_SESSION_H_

#include "net/base/net_export.h"

namespace net {

class HostResolver;

// This class holds session objects used by FtpNetworkTransaction objects.
class NET_EXPORT_PRIVATE FtpNetworkSession {
 public:
  explicit FtpNetworkSession(HostResolver* host_resolver);
  virtual ~FtpNetworkSession();

  HostResolver* host_resolver() { return host_resolver_; }

 private:
  HostResolver* const host_resolver_;
};

}  // namespace net

#endif  // NET_FTP_FTP_NETWORK_SESSION_H_
