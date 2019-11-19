// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_SETTINGS_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_SETTINGS_MOJOM_TRAITS_H_

#include <utility>

#include "components/viz/common/resources/resource_settings.h"
#include "services/viz/public/mojom/compositing/resource_settings.mojom.h"
#include "ui/gfx/mojom/buffer_types.mojom.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::ResourceSettingsDataView,
                    viz::ResourceSettings> {
  static bool use_gpu_memory_buffer_resources(
      const viz::ResourceSettings& input) {
    return input.use_gpu_memory_buffer_resources;
  }

  static bool Read(viz::mojom::ResourceSettingsDataView data,
                   viz::ResourceSettings* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_SETTINGS_MOJOM_TRAITS_H_
