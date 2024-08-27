// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SERVICE_URLS_H_
#define REMOTING_BASE_SERVICE_URLS_H_

#include <string>

#include "base/memory/singleton.h"

namespace remoting {

// This class contains the URLs to the services used by the host (except for
// Gaia, which has its own GaiaUrls class. In debug builds, it allows these URLs
// to be overriden by command line switches, allowing the host process to be
// pointed at alternate/test servers.
class ServiceUrls {
 public:
  static ServiceUrls* GetInstance();

  ServiceUrls(const ServiceUrls&) = delete;
  ServiceUrls& operator=(const ServiceUrls&) = delete;

  const std::string& ftl_server_endpoint() const {
    return ftl_server_endpoint_;
  }

  const std::string& remoting_cloud_endpoint() const {
    return remoting_cloud_endpoint_;
  }

  const std::string& remoting_corp_endpoint() const {
    return remoting_corp_endpoint_;
  }

  const std::string& remoting_server_endpoint() const {
    return remoting_server_endpoint_;
  }

 private:
  friend struct base::DefaultSingletonTraits<ServiceUrls>;

  ServiceUrls();
  virtual ~ServiceUrls();

  std::string ftl_server_endpoint_;
  std::string remoting_cloud_endpoint_;
  std::string remoting_corp_endpoint_;
  std::string remoting_server_endpoint_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_SERVICE_URLS_H_
