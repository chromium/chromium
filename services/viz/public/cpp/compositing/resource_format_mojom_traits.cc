// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/resource_format_mojom_traits.h"

namespace mojo {

// static
viz::mojom::ResourceFormat
EnumTraits<viz::mojom::ResourceFormat, viz::ResourceFormat>::ToMojom(
    viz::ResourceFormat format) {
  switch (format) {
    case viz::ResourceFormat::RGBA_8888:
      return viz::mojom::ResourceFormat::RGBA_8888;
    case viz::ResourceFormat::RGBA_4444:
      return viz::mojom::ResourceFormat::RGBA_4444;
    case viz::ResourceFormat::BGRA_8888:
      return viz::mojom::ResourceFormat::BGRA_8888;
    case viz::ResourceFormat::ALPHA_8:
      return viz::mojom::ResourceFormat::ALPHA_8;
    case viz::ResourceFormat::LUMINANCE_8:
      return viz::mojom::ResourceFormat::LUMINANCE_8;
    case viz::ResourceFormat::RGB_565:
      return viz::mojom::ResourceFormat::RGB_565;
    case viz::ResourceFormat::BGR_565:
      return viz::mojom::ResourceFormat::BGR_565;
    case viz::ResourceFormat::ETC1:
      return viz::mojom::ResourceFormat::ETC1;
    case viz::ResourceFormat::RED_8:
      return viz::mojom::ResourceFormat::RED_8;
    case viz::ResourceFormat::RG_88:
      return viz::mojom::ResourceFormat::RG_88;
    case viz::ResourceFormat::LUMINANCE_F16:
      return viz::mojom::ResourceFormat::LUMINANCE_F16;
    case viz::ResourceFormat::RGBA_F16:
      return viz::mojom::ResourceFormat::RGBA_F16;
    case viz::ResourceFormat::R16_EXT:
      return viz::mojom::ResourceFormat::R16_EXT;
    case viz::ResourceFormat::RG16_EXT:
      return viz::mojom::ResourceFormat::RG16_EXT;
    case viz::ResourceFormat::RGBX_8888:
      return viz::mojom::ResourceFormat::RGBX_8888;
    case viz::ResourceFormat::BGRX_8888:
      return viz::mojom::ResourceFormat::BGRX_8888;
    case viz::ResourceFormat::RGBA_1010102:
      return viz::mojom::ResourceFormat::RGBX_1010102;
    case viz::ResourceFormat::BGRA_1010102:
      return viz::mojom::ResourceFormat::BGRX_1010102;
    case viz::ResourceFormat::YVU_420:
      return viz::mojom::ResourceFormat::YVU_420;
    case viz::ResourceFormat::YUV_420_BIPLANAR:
      return viz::mojom::ResourceFormat::YUV_420_BIPLANAR;
    case viz::ResourceFormat::YUVA_420_TRIPLANAR:
      return viz::mojom::ResourceFormat::YUVA_420_TRIPLANAR;
    case viz::ResourceFormat::P010:
      return viz::mojom::ResourceFormat::P010;
  }
  NOTREACHED();
  return viz::mojom::ResourceFormat::RGBA_8888;
}

// static
bool EnumTraits<viz::mojom::ResourceFormat, viz::ResourceFormat>::FromMojom(
    viz::mojom::ResourceFormat format,
    viz::ResourceFormat* out) {
  switch (format) {
    case viz::mojom::ResourceFormat::RGBA_8888:
      *out = viz::ResourceFormat::RGBA_8888;
      return true;
    case viz::mojom::ResourceFormat::RGBA_4444:
      *out = viz::ResourceFormat::RGBA_4444;
      return true;
    case viz::mojom::ResourceFormat::BGRA_8888:
      *out = viz::ResourceFormat::BGRA_8888;
      return true;
    case viz::mojom::ResourceFormat::ALPHA_8:
      *out = viz::ResourceFormat::ALPHA_8;
      return true;
    case viz::mojom::ResourceFormat::LUMINANCE_8:
      *out = viz::ResourceFormat::LUMINANCE_8;
      return true;
    case viz::mojom::ResourceFormat::RGB_565:
      *out = viz::ResourceFormat::RGB_565;
      return true;
    case viz::mojom::ResourceFormat::BGR_565:
      *out = viz::ResourceFormat::BGR_565;
      return true;
    case viz::mojom::ResourceFormat::ETC1:
      *out = viz::ResourceFormat::ETC1;
      return true;
    case viz::mojom::ResourceFormat::RED_8:
      *out = viz::ResourceFormat::RED_8;
      return true;
    case viz::mojom::ResourceFormat::RG_88:
      *out = viz::ResourceFormat::RG_88;
      return true;
    case viz::mojom::ResourceFormat::LUMINANCE_F16:
      *out = viz::ResourceFormat::LUMINANCE_F16;
      return true;
    case viz::mojom::ResourceFormat::RGBA_F16:
      *out = viz::ResourceFormat::RGBA_F16;
      return true;
    case viz::mojom::ResourceFormat::R16_EXT:
      *out = viz::ResourceFormat::R16_EXT;
      return true;
    case viz::mojom::ResourceFormat::RG16_EXT:
      *out = viz::ResourceFormat::RG16_EXT;
      return true;
    case viz::mojom::ResourceFormat::RGBX_8888:
      *out = viz::ResourceFormat::RGBX_8888;
      return true;
    case viz::mojom::ResourceFormat::BGRX_8888:
      *out = viz::ResourceFormat::BGRX_8888;
      return true;
    case viz::mojom::ResourceFormat::RGBX_1010102:
      *out = viz::ResourceFormat::RGBA_1010102;
      return true;
    case viz::mojom::ResourceFormat::BGRX_1010102:
      *out = viz::ResourceFormat::BGRA_1010102;
      return true;
    case viz::mojom::ResourceFormat::YVU_420:
      *out = viz::ResourceFormat::YVU_420;
      return true;
    case viz::mojom::ResourceFormat::YUV_420_BIPLANAR:
      *out = viz::ResourceFormat::YUV_420_BIPLANAR;
      return true;
    case viz::mojom::ResourceFormat::YUVA_420_TRIPLANAR:
      *out = viz::ResourceFormat::YUVA_420_TRIPLANAR;
      return true;
    case viz::mojom::ResourceFormat::P010:
      *out = viz::ResourceFormat::P010;
      return true;
  }

  return false;
}

}  // namespace mojo
