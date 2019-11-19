// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_IMAGE_INFO_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_IMAGE_INFO_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "skia/public/mojom/image_info.mojom-shared.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(SKIA_SHARED_TRAITS)
    StructTraits<skia::mojom::ImageInfoDataView, SkImageInfo> {
  static skia::mojom::ColorType color_type(const SkImageInfo& info);
  static skia::mojom::AlphaType alpha_type(const SkImageInfo& info);
  static std::vector<uint8_t> serialized_color_space(const SkImageInfo& info);
  static uint32_t width(const SkImageInfo& info);
  static uint32_t height(const SkImageInfo& info);
  static bool Read(skia::mojom::ImageInfoDataView data, SkImageInfo* info);
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_IMAGE_INFO_MOJOM_TRAITS_H_
