// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_HTTP_SERVER_PROPERTIES_FACTORY_H_
#define IOS_CHROME_BROWSER_NET_MODEL_HTTP_SERVER_PROPERTIES_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"

class JsonPrefStore;

namespace net {
class HttpServerProperties;
class NetLog;
}  // namespace net

// Class for registration and creation of HttpServerProperties.
class HttpServerPropertiesFactory {
 public:
  HttpServerPropertiesFactory(const HttpServerPropertiesFactory&) = delete;
  HttpServerPropertiesFactory& operator=(const HttpServerPropertiesFactory&) =
      delete;

  // Create an instance of HttpServerProperties.
  static std::unique_ptr<net::HttpServerProperties> CreateHttpServerProperties(
      scoped_refptr<JsonPrefStore> pref_store,
      net::NetLog* net_log);
};

#endif  // IOS_CHROME_BROWSER_NET_MODEL_HTTP_SERVER_PROPERTIES_FACTORY_H_
