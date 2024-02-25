// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/color_provider_color_maps_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::ColorProviderColorMapsDataView,
                  blink::ColorProviderColorMaps>::
    Read(blink::mojom::ColorProviderColorMapsDataView data,
         blink::ColorProviderColorMaps* out_colors) {
  return data.ReadLightColorsMap(&out_colors->light_colors_map) &&
         data.ReadDarkColorsMap(&out_colors->dark_colors_map) &&
         data.ReadForcedColorsMap(&out_colors->forced_colors_map);
}

}  // namespace mojo
