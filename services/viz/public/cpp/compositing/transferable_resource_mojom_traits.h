// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/common/vulkan_ycbcr_info_mojom_traits.h"
#include "services/viz/public/mojom/compositing/transferable_resource.mojom-shared.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::TransferableResourceDataView,
                    viz::TransferableResource> {
  static uint32_t id(const viz::TransferableResource& resource) {
    return resource.id;
  }

  static viz::mojom::ResourceFormat format(
      const viz::TransferableResource& resource) {
    return static_cast<viz::mojom::ResourceFormat>(resource.format);
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

  static bool read_lock_fences_enabled(
      const viz::TransferableResource& resource) {
    return resource.read_lock_fences_enabled;
  }

  static bool is_software(const viz::TransferableResource& resource) {
    return resource.is_software;
  }

  static bool is_overlay_candidate(const viz::TransferableResource& resource) {
    return resource.is_overlay_candidate;
  }

  static bool is_backed_by_surface_texture(
      const viz::TransferableResource& resource) {
#if defined(OS_ANDROID)
    // TransferableResource has this in an #ifdef, but mojo doesn't let us.
    // TODO(https://crbug.com/671901)
    return resource.is_backed_by_surface_texture;
#else
    return false;
#endif
  }

  static bool wants_promotion_hint(const viz::TransferableResource& resource) {
#if defined(OS_ANDROID)
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

  static const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info(
      const viz::TransferableResource& resource) {
    return resource.ycbcr_info;
  }

  static bool Read(viz::mojom::TransferableResourceDataView data,
                   viz::TransferableResource* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_TRANSFERABLE_RESOURCE_MOJOM_TRAITS_H_
