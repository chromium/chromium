// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_COOKIE_SETTING_OVERRIDES_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_COOKIE_SETTING_OVERRIDES_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/network/public/mojom/cookie_setting_overrides.mojom.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::CookieSettingOverridesDataView,
                 net::CookieSettingOverrides> {
  static uint64_t overrides_bitmask(const net::CookieSettingOverrides& input) {
    return input.ToEnumBitmask();
  }

  static bool Read(network::mojom::CookieSettingOverridesDataView data,
                   net::CookieSettingOverrides* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_COOKIE_SETTING_OVERRIDES_MOJOM_TRAITS_H_
