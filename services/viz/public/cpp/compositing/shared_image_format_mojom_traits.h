// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_IMAGE_FORMAT_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_IMAGE_FORMAT_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "services/viz/public/mojom/compositing/shared_image_format.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::PlaneConfig,
                  viz::SharedImageFormat::PlaneConfig> {
  static viz::mojom::PlaneConfig ToMojom(
      viz::SharedImageFormat::PlaneConfig plane_config);

  static bool FromMojom(viz::mojom::PlaneConfig input,
                        viz::SharedImageFormat::PlaneConfig* out);
};

template <>
struct EnumTraits<viz::mojom::Subsampling,
                  viz::SharedImageFormat::Subsampling> {
  static viz::mojom::Subsampling ToMojom(
      viz::SharedImageFormat::Subsampling subsampling);

  static bool FromMojom(viz::mojom::Subsampling input,
                        viz::SharedImageFormat::Subsampling* out);
};

template <>
struct EnumTraits<viz::mojom::ChannelFormat,
                  viz::SharedImageFormat::ChannelFormat> {
  static viz::mojom::ChannelFormat ToMojom(
      viz::SharedImageFormat::ChannelFormat channel_format);

  static bool FromMojom(viz::mojom::ChannelFormat input,
                        viz::SharedImageFormat::ChannelFormat* out);
};

template <>
struct StructTraits<
    viz::mojom::MultiplanarFormatDataView,
    viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat> {
  static viz::SharedImageFormat::PlaneConfig plane_config(
      viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat
          format) {
    return format.plane_config;
  }

  static viz::SharedImageFormat::Subsampling subsampling(
      viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat
          format) {
    return format.subsampling;
  }

  static viz::SharedImageFormat::ChannelFormat channel_format(
      viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat
          format) {
    return format.channel_format;
  }

#if BUILDFLAG(IS_OZONE)
  static bool prefers_external_sampler(
      viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat
          format) {
    return format.prefers_external_sampler;
  }
#endif

  static bool Read(
      viz::mojom::MultiplanarFormatDataView data,
      viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat* out);
};

template <>
struct UnionTraits<viz::mojom::SharedImageFormatDataView,
                   viz::SharedImageFormat> {
 public:
  static viz::mojom::SharedImageFormatDataView::Tag GetTag(
      const viz::SharedImageFormat& format) {
    CHECK_NE(format.plane_type(), viz::SharedImageFormat::PlaneType::kUnknown);
    if (format.is_multi_plane())
      return viz::mojom::SharedImageFormatDataView::Tag::kMultiplanarFormat;
    else
      return viz::mojom::SharedImageFormatDataView::Tag::kSingleplanarFormat;
  }

  static viz::mojom::SingleplanarFormat singleplanar_format(
      const viz::SharedImageFormat& format) {
    return format.singleplanar_format();
  }
  static viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat
  multiplanar_format(const viz::SharedImageFormat& format) {
    return format.multiplanar_format();
  }

  static bool Read(viz::mojom::SharedImageFormatDataView data,
                   viz::SharedImageFormat* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_SHARED_IMAGE_FORMAT_MOJOM_TRAITS_H_
