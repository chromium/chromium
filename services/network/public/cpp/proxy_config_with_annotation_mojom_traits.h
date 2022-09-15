// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_WITH_ANNOTATION_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_WITH_ANNOTATION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/mutable_network_traffic_annotation_tag_mojom_traits.h"
#include "services/network/public/cpp/proxy_config_mojom_traits.h"
#include "services/network/public/mojom/proxy_config_with_annotation.mojom-shared.h"

// This file handles the serialization of net::ProxyConfigWithAnnotation.

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    StructTraits<network::mojom::ProxyConfigWithAnnotationDataView,
                 net::ProxyConfigWithAnnotation> {
 public:
  static const net::ProxyConfig& value(
      const net::ProxyConfigWithAnnotation& r) {
    return r.value();
  }
  static const net::MutableNetworkTrafficAnnotationTag traffic_annotation(
      const net::ProxyConfigWithAnnotation& r) {
    return net::MutableNetworkTrafficAnnotationTag(r.traffic_annotation());
  }

  static bool Read(network::mojom::ProxyConfigWithAnnotationDataView data,
                   net::ProxyConfigWithAnnotation* out_proxy_config);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_WITH_ANNOTATION_MOJOM_TRAITS_H_
