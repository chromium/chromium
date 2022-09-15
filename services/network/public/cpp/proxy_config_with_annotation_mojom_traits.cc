// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/proxy_config_with_annotation_mojom_traits.h"

namespace mojo {

bool StructTraits<network::mojom::ProxyConfigWithAnnotationDataView,
                  net::ProxyConfigWithAnnotation>::
    Read(network::mojom::ProxyConfigWithAnnotationDataView data,
         net::ProxyConfigWithAnnotation* out_proxy_config) {
  net::ProxyConfig config;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  if (!data.ReadValue(&config) ||
      !data.ReadTrafficAnnotation(&traffic_annotation)) {
    return false;
  }
  *out_proxy_config = net::ProxyConfigWithAnnotation(
      config, net::NetworkTrafficAnnotationTag(traffic_annotation));
  return true;
}

}  // namespace mojo
