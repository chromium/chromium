// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_ISOLATION_KEY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_ISOLATION_KEY_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/schemeful_site.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/public/mojom/shared_dictionary_isolation_key.mojom-shared.h"
#include "url/origin.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::SharedDictionaryIsolationKeyDataView,
                    net::SharedDictionaryIsolationKey> {
  static const url::Origin& frame_origin(
      const net::SharedDictionaryIsolationKey& obj) {
    return obj.frame_origin();
  }

  static const net::SchemefulSite& top_frame_site(
      const net::SharedDictionaryIsolationKey& obj) {
    return obj.top_frame_site();
  }

  static bool Read(network::mojom::SharedDictionaryIsolationKeyDataView data,
                   net::SharedDictionaryIsolationKey* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_DICTIONARY_ISOLATION_KEY_MOJOM_TRAITS_H_
