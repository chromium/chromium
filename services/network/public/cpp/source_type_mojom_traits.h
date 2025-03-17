// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SOURCE_TYPE_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SOURCE_TYPE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/filter/source_stream_type.h"
#include "services/network/public/mojom/source_type.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::SourceType, net::SourceStreamType> {
  static network::mojom::SourceType ToMojom(net::SourceStreamType type);
  static bool FromMojom(network::mojom::SourceType in,
                        net::SourceStreamType* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SOURCE_TYPE_MOJOM_TRAITS_H_
