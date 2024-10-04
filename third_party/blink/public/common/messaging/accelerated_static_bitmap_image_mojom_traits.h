// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_STATIC_BITMAP_IMAGE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_STATIC_BITMAP_IMAGE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/messaging/accelerated_image_info.h"
#include "third_party/blink/public/mojom/messaging/static_bitmap_image.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::SharedImageUsageSet::DataView,
                 gpu::SharedImageUsageSet> {
  static uint32_t usage(const gpu::SharedImageUsageSet& input) {
    return uint32_t(input);
  }

  static bool Read(blink::mojom::SharedImageUsageSet::DataView data,
                   gpu::SharedImageUsageSet* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::AcceleratedStaticBitmapImage::DataView,
                 blink::AcceleratedImageInfo> {
  static const gpu::ExportedSharedImage& shared_image(
      const blink::AcceleratedImageInfo& input) {
    return input.shared_image;
  }

  static SkImageInfo image_info(const blink::AcceleratedImageInfo& input) {
    return input.image_info;
  }

  static bool is_origin_top_left(const blink::AcceleratedImageInfo& input) {
    return input.is_origin_top_left;
  }

  static bool supports_display_compositing(
      const blink::AcceleratedImageInfo& input) {
    return input.supports_display_compositing;
  }

  static bool is_overlay_candidate(const blink::AcceleratedImageInfo& input) {
    return input.is_overlay_candidate;
  }

  static mojo::PendingRemote<blink::mojom::ImageReleaseCallback>
  release_callback(blink::AcceleratedImageInfo& input);

  static bool Read(blink::mojom::AcceleratedStaticBitmapImage::DataView data,
                   blink::AcceleratedImageInfo* out);
};
}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_STATIC_BITMAP_IMAGE_MOJOM_TRAITS_H_
