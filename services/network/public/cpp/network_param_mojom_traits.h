// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/network_param.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace mojo {

template <>
class StructTraits<network::mojom::HttpVersionDataView, net::HttpVersion> {
 public:
  static int16_t major_value(net::HttpVersion version) {
    return version.major_value();
  }
  static int16_t minor_value(net::HttpVersion version) {
    return version.minor_value();
  }

  static bool Read(network::mojom::HttpVersionDataView data,
                   net::HttpVersion* out);
};

#if defined(OS_ANDROID)
template <>
struct EnumTraits<network::mojom::ApplicationState,
                  base::android::ApplicationState> {
  static network::mojom::ApplicationState ToMojom(
      base::android::ApplicationState input);
  static bool FromMojom(network::mojom::ApplicationState input,
                        base::android::ApplicationState* output);
};
#endif

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_
