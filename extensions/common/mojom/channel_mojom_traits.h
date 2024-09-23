// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_CHANNEL_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_CHANNEL_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "components/version_info/channel.h"
#include "extensions/common/mojom/channel.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<extensions::mojom::Channel, version_info::Channel> {
  static extensions::mojom::Channel ToMojom(version_info::Channel input) {
    switch (input) {
      case version_info::Channel::UNKNOWN:
        return extensions::mojom::Channel::kUnknown;
      case version_info::Channel::CANARY:
        return extensions::mojom::Channel::kCanary;
      case version_info::Channel::DEV:
        return extensions::mojom::Channel::kDev;
      case version_info::Channel::BETA:
        return extensions::mojom::Channel::kBeta;
      case version_info::Channel::STABLE:
        return extensions::mojom::Channel::kStable;
    }
    NOTREACHED_IN_MIGRATION();
    return extensions::mojom::Channel::kUnknown;
  }

  static bool FromMojom(extensions::mojom::Channel input,
                        version_info::Channel* out) {
    switch (input) {
      case extensions::mojom::Channel::kUnknown:
        *out = version_info::Channel::UNKNOWN;
        return true;
      case extensions::mojom::Channel::kCanary:
        *out = version_info::Channel::CANARY;
        return true;
      case extensions::mojom::Channel::kDev:
        *out = version_info::Channel::DEV;
        return true;
      case extensions::mojom::Channel::kBeta:
        *out = version_info::Channel::BETA;
        return true;
      case extensions::mojom::Channel::kStable:
        *out = version_info::Channel::STABLE;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    *out = version_info::Channel::UNKNOWN;
    return false;
  }
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_CHANNEL_MOJOM_TRAITS_H_
