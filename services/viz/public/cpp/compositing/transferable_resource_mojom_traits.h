// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/common/vulkan_ycbcr_info_mojom_traits.h"
#include "services/viz/public/cpp/compositing/shared_image_format_mojom_traits.h"
#include "services/viz/public/mojom/compositing/transferable_resource.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::SynchronizationType,
                  viz::TransferableResource::SynchronizationType> {
  static viz::mojom::SynchronizationType ToMojom(
      viz::TransferableResource::SynchronizationType type);

  static bool FromMojom(viz::mojom::SynchronizationType input,
                        viz::TransferableResource::SynchronizationType* out);
};

template <>
struct StructTraits<viz::mojom::TransferableResourceDataView,
                    viz::TransferableResource> {
  static const viz::ResourceId& id(const viz::TransferableResource& resource) {
    return resource.id;
  }

  static viz::SharedImageFormat format(
      const viz::TransferableResource& resource) {
    return resource.format;
  }

  static uint32_t filter(const viz::TransferableResource& resource) {
    return resource.filter;
  }

  static gfx::Size size(const viz::TransferableResource& resource) {
    return resource.size;
  }

  static const gpu::MailboxHolder& mailbox_holder(
      const viz::TransferableResource& resource) {
    return resource.mailbox_holder;
  }

  static viz::TransferableResource::SynchronizationType synchronization_type(
      const viz::TransferableResource& resource) {
    return resource.synchronization_type;
  }

  static bool is_software(const viz::TransferableResource& resource) {
    return resource.is_software;
  }

  static bool is_overlay_candidate(const viz::TransferableResource& resource) {
    return resource.is_overlay_candidate;
  }

  static bool is_backed_by_surface_texture(
      const viz::TransferableResource& resource) {
#if BUILDFLAG(IS_ANDROID)
    // TransferableResource has this in an #ifdef, but mojo doesn't let us.
    // TODO(https://crbug.com/671901)
    return resource.is_backed_by_surface_texture;
#else
    return false;
#endif
  }

  static bool wants_promotion_hint(const viz::TransferableResource& resource) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
    // TransferableResource has this in an #ifdef, but mojo doesn't let us.
    // TODO(https://crbug.com/671901)
    return resource.wants_promotion_hint;
#else
    return false;
#endif
  }

  static const gfx::ColorSpace& color_space(
      const viz::TransferableResource& resource) {
    return resource.color_space;
  }

  static const absl::optional<gfx::ColorSpace>& color_space_when_sampled(
      const viz::TransferableResource& resource) {
    return resource.color_space_when_sampled;
  }

  static const absl::optional<gfx::HDRMetadata>& hdr_metadata(
      const viz::TransferableResource& resource) {
    return resource.hdr_metadata;
  }

  static const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info(
      const viz::TransferableResource& resource) {
    return resource.ycbcr_info;
  }

  static bool Read(viz::mojom::TransferableResourceDataView data,
                   viz::TransferableResource* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_
