// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_with_annotation.h"

namespace net {

namespace {

constexpr NetworkTrafficAnnotationTag kDirectProxyTrafficAnnotation =
    DefineNetworkTrafficAnnotation("proxy_config_direct", R"(
    semantics {
      sender: "Proxy Config"
      description:
        "Direct connections are being used instead of a proxy. This is a place "
        "holder annotation that would include details about where the "
        "configuration, which can trigger fetching a PAC file, came from."
      trigger:
        "Connecting directly to destination sites instead of using a proxy is "
        "the default behavior."
      data:
        "None."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: NO
      setting:
        "This isn't a real network request. A proxy can be selected in "
        "settings."
      policy_exception_justification:
        "Using either of 'ProxyMode', 'ProxyServer', or 'ProxyPacUrl' policies "
        "can set Chrome to use a specific proxy settings and avoid directly "
        "connecting to the websites."
    })");

}  // namespace

ProxyConfigWithAnnotation::ProxyConfigWithAnnotation()
    : value_(ProxyConfig::CreateDirect()),
      traffic_annotation_(
          MutableNetworkTrafficAnnotationTag(kDirectProxyTrafficAnnotation)) {}

ProxyConfigWithAnnotation::ProxyConfigWithAnnotation(
    const ProxyConfig& proxy_config,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : value_(proxy_config),
      traffic_annotation_(
          MutableNetworkTrafficAnnotationTag(traffic_annotation)) {}

}  // namespace net
