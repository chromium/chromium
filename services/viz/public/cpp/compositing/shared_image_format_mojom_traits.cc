// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/shared_image_format_mojom_traits.h"

namespace mojo {

// static
viz::mojom::PlaneConfig
EnumTraits<viz::mojom::PlaneConfig, viz::SharedImageFormat::PlaneConfig>::
    ToMojom(viz::SharedImageFormat::PlaneConfig plane_config) {
  switch (plane_config) {
    case viz::SharedImageFormat::PlaneConfig::kY_U_V:
      return viz::mojom::PlaneConfig::kY_U_V;
    case viz::SharedImageFormat::PlaneConfig::kY_V_U:
      return viz::mojom::PlaneConfig::kY_V_U;
    case viz::SharedImageFormat::PlaneConfig::kY_UV:
      return viz::mojom::PlaneConfig::kY_UV;
    case viz::SharedImageFormat::PlaneConfig::kY_UV_A:
      return viz::mojom::PlaneConfig::kY_UV_A;
    case viz::SharedImageFormat::PlaneConfig::kY_U_V_A:
      return viz::mojom::PlaneConfig::kY_U_V_A;
  }
  NOTREACHED_IN_MIGRATION();
  return viz::mojom::PlaneConfig::kY_UV;
}

// static
bool EnumTraits<viz::mojom::PlaneConfig, viz::SharedImageFormat::PlaneConfig>::
    FromMojom(viz::mojom::PlaneConfig input,
              viz::SharedImageFormat::PlaneConfig* out) {
  switch (input) {
    case viz::mojom::PlaneConfig::kY_U_V:
      *out = viz::SharedImageFormat::PlaneConfig::kY_U_V;
      return true;
    case viz::mojom::PlaneConfig::kY_V_U:
      *out = viz::SharedImageFormat::PlaneConfig::kY_V_U;
      return true;
    case viz::mojom::PlaneConfig::kY_UV:
      *out = viz::SharedImageFormat::PlaneConfig::kY_UV;
      return true;
    case viz::mojom::PlaneConfig::kY_UV_A:
      *out = viz::SharedImageFormat::PlaneConfig::kY_UV_A;
      return true;
    case viz::mojom::PlaneConfig::kY_U_V_A:
      *out = viz::SharedImageFormat::PlaneConfig::kY_U_V_A;
      return true;
  }
  return false;
}

// static
viz::mojom::Subsampling
EnumTraits<viz::mojom::Subsampling, viz::SharedImageFormat::Subsampling>::
    ToMojom(viz::SharedImageFormat::Subsampling subsampling) {
  switch (subsampling) {
    case viz::SharedImageFormat::Subsampling::k420:
      return viz::mojom::Subsampling::k420;
    case viz::SharedImageFormat::Subsampling::k422:
      return viz::mojom::Subsampling::k422;
    case viz::SharedImageFormat::Subsampling::k444:
      return viz::mojom::Subsampling::k444;
  }
  NOTREACHED_IN_MIGRATION();
  return viz::mojom::Subsampling::k420;
}

// static
bool EnumTraits<viz::mojom::Subsampling, viz::SharedImageFormat::Subsampling>::
    FromMojom(viz::mojom::Subsampling input,
              viz::SharedImageFormat::Subsampling* out) {
  switch (input) {
    case viz::mojom::Subsampling::k420:
      *out = viz::SharedImageFormat::Subsampling::k420;
      return true;
    case viz::mojom::Subsampling::k422:
      *out = viz::SharedImageFormat::Subsampling::k422;
      return true;
    case viz::mojom::Subsampling::k444:
      *out = viz::SharedImageFormat::Subsampling::k444;
      return true;
  }
  return false;
}

// static
viz::mojom::ChannelFormat
EnumTraits<viz::mojom::ChannelFormat, viz::SharedImageFormat::ChannelFormat>::
    ToMojom(viz::SharedImageFormat::ChannelFormat channel_format) {
  switch (channel_format) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return viz::mojom::ChannelFormat::k8;
    case viz::SharedImageFormat::ChannelFormat::k10:
      return viz::mojom::ChannelFormat::k10;
    case viz::SharedImageFormat::ChannelFormat::k16:
      return viz::mojom::ChannelFormat::k16;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return viz::mojom::ChannelFormat::k16F;
  }
  NOTREACHED_IN_MIGRATION();
  return viz::mojom::ChannelFormat::k8;
}

// static
bool EnumTraits<viz::mojom::ChannelFormat,
                viz::SharedImageFormat::ChannelFormat>::
    FromMojom(viz::mojom::ChannelFormat input,
              viz::SharedImageFormat::ChannelFormat* out) {
  switch (input) {
    case viz::mojom::ChannelFormat::k8:
      *out = viz::SharedImageFormat::ChannelFormat::k8;
      return true;
    case viz::mojom::ChannelFormat::k10:
      *out = viz::SharedImageFormat::ChannelFormat::k10;
      return true;
    case viz::mojom::ChannelFormat::k16:
      *out = viz::SharedImageFormat::ChannelFormat::k16;
      return true;
    case viz::mojom::ChannelFormat::k16F:
      *out = viz::SharedImageFormat::ChannelFormat::k16F;
      return true;
  }
  return false;
}

// static
bool StructTraits<
    viz::mojom::MultiplanarFormatDataView,
    viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat>::
    Read(viz::mojom::MultiplanarFormatDataView data,
         viz::SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat*
             out) {
  if (!data.ReadPlaneConfig(&out->plane_config))
    return false;
  if (!data.ReadSubsampling(&out->subsampling))
    return false;
  if (!data.ReadChannelFormat(&out->channel_format))
    return false;
#if BUILDFLAG(IS_OZONE)
  out->prefers_external_sampler = data.prefers_external_sampler();
#endif

  return true;
}

bool UnionTraits<
    viz::mojom::SharedImageFormatDataView,
    viz::SharedImageFormat>::Read(viz::mojom::SharedImageFormatDataView data,
                                  viz::SharedImageFormat* out) {
  switch (data.tag()) {
    case viz::mojom::SharedImageFormatDataView::Tag::kSingleplanarFormat:
      if (!data.ReadSingleplanarFormat(&out->format_.singleplanar_format)) {
        return false;
      }
      out->plane_type_ = viz::SharedImageFormat::PlaneType::kSinglePlane;
      return true;
    case viz::mojom::SharedImageFormatDataView::Tag::kMultiplanarFormat:
      if (!data.ReadMultiplanarFormat(&out->format_.multiplanar_format))
        return false;
      out->plane_type_ = viz::SharedImageFormat::PlaneType::kMultiPlane;
      return true;
  }
  return false;
}

}  // namespace mojo
