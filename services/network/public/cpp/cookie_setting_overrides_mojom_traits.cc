// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_setting_overrides_mojom_traits.h"

namespace mojo {

bool StructTraits<network::mojom::CookieSettingOverridesDataView,
                  net::CookieSettingOverrides>::
    Read(network::mojom::CookieSettingOverridesDataView data,
         net::CookieSettingOverrides* out) {
  // FromEnumBitmask ignores any extra bits.
  *out = net::CookieSettingOverrides::FromEnumBitmask(data.overrides_bitmask());
  return true;
}

}  // namespace mojo
