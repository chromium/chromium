// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_MUTABLE_PARTIAL_NETWORK_TRAFFIC_ANNOTATION_TAG_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_MUTABLE_PARTIAL_NETWORK_TRAFFIC_ANNOTATION_TAG_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/mutable_partial_network_traffic_annotation_tag.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<
    network::mojom::MutablePartialNetworkTrafficAnnotationTagDataView,
    net::MutablePartialNetworkTrafficAnnotationTag> {
  static int32_t unique_id_hash_code(
      const net::MutablePartialNetworkTrafficAnnotationTag&
          traffic_annotation) {
    return traffic_annotation.unique_id_hash_code;
  }
  static int32_t completing_id_hash_code(
      const net::MutablePartialNetworkTrafficAnnotationTag&
          traffic_annotation) {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
    return traffic_annotation.completing_id_hash_code;
#else
    return -1;
#endif
  }
  static bool Read(
      network::mojom::MutablePartialNetworkTrafficAnnotationTagDataView data,
      net::MutablePartialNetworkTrafficAnnotationTag* out) {
    out->unique_id_hash_code = data.unique_id_hash_code();
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
    out->completing_id_hash_code = data.completing_id_hash_code();
#endif
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_MUTABLE_PARTIAL_NETWORK_TRAFFIC_ANNOTATION_TAG_MOJOM_TRAITS_H_
