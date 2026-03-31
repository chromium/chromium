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
    NOTREACHED();
  }

  static version_info::Channel FromMojom(extensions::mojom::Channel input) {
    switch (input) {
      case extensions::mojom::Channel::kUnknown:
        return version_info::Channel::UNKNOWN;
      case extensions::mojom::Channel::kCanary:
        return version_info::Channel::CANARY;
      case extensions::mojom::Channel::kDev:
        return version_info::Channel::DEV;
      case extensions::mojom::Channel::kBeta:
        return version_info::Channel::BETA;
      case extensions::mojom::Channel::kStable:
        return version_info::Channel::STABLE;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_CHANNEL_MOJOM_TRAITS_H_
