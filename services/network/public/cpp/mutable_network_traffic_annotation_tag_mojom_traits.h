// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_MUTABLE_NETWORK_TRAFFIC_ANNOTATION_TAG_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_MUTABLE_NETWORK_TRAFFIC_ANNOTATION_TAG_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/mutable_network_traffic_annotation_tag.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::MutableNetworkTrafficAnnotationTagDataView,
                    net::MutableNetworkTrafficAnnotationTag> {
  static int32_t unique_id_hash_code(
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
    return traffic_annotation.unique_id_hash_code;
  }
  static bool Read(
      network::mojom::MutableNetworkTrafficAnnotationTagDataView data,
      net::MutableNetworkTrafficAnnotationTag* out) {
    out->unique_id_hash_code = data.unique_id_hash_code();
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_MUTABLE_NETWORK_TRAFFIC_ANNOTATION_TAG_MOJOM_TRAITS_H_
