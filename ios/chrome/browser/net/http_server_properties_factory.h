// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_FACTORY_H_
#define IOS_CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

class JsonPrefStore;

namespace net {
class HttpServerProperties;
class NetLog;
}  // namespace net

// Class for registration and creation of HttpServerProperties.
class HttpServerPropertiesFactory {
 public:
  // Create an instance of HttpServerProperties.
  static std::unique_ptr<net::HttpServerProperties> CreateHttpServerProperties(
      scoped_refptr<JsonPrefStore> pref_store,
      net::NetLog* net_log);

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesFactory);
};

#endif  // IOS_CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_FACTORY_H_
