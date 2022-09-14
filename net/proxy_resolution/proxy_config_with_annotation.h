// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_CONFIG_WITH_ANNOTATION_H_
#define NET_PROXY_RESOLUTION_PROXY_CONFIG_WITH_ANNOTATION_H_

#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

// ProxyConfigWithAnnotation encapsulates a ProxyConfig with the network traffic
// annotation that specifies the source of proxy config.
class NET_EXPORT ProxyConfigWithAnnotation {
 public:
  // Creates a Direct proxy config.
  ProxyConfigWithAnnotation();

  ProxyConfigWithAnnotation(
      const ProxyConfig& proxy_config,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  static ProxyConfigWithAnnotation CreateDirect() {
    return ProxyConfigWithAnnotation();
  }

  NetworkTrafficAnnotationTag traffic_annotation() const {
    return NetworkTrafficAnnotationTag(traffic_annotation_);
  }

  const ProxyConfig& value() const { return value_; }

 private:
  ProxyConfig value_;
  MutableNetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_CONFIG_WITH_ANNOTATION_H_
