// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_FRAME_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_FRAME_MOJOM_TRAITS_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/common/mailbox_holder_mojom_traits.h"
#include "gpu/ipc/common/vulkan_ycbcr_info_mojom_traits.h"
#include "media/base/ipc/media_param_traits_macros.h"
#include "media/base/video_frame.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::SharedImageFormatType,
                  media::SharedImageFormatType> {
  static media::mojom::SharedImageFormatType ToMojom(
      media::SharedImageFormatType type);

  static bool FromMojom(media::mojom::SharedImageFormatType input,
                        media::SharedImageFormatType* out);
};

template <>
struct StructTraits<media::mojom::VideoFrameDataView,
                    scoped_refptr<media::VideoFrame>> {
  static bool IsNull(const scoped_refptr<media::VideoFrame>& input) {
    return !input;
  }

  static void SetToNull(scoped_refptr<media::VideoFrame>* input) {
    *input = nullptr;
  }

  static media::VideoPixelFormat format(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->format();
  }

  static const gfx::Size& coded_size(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->coded_size();
  }

  static const gfx::Rect& visible_rect(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->visible_rect();
  }

  static const gfx::Size& natural_size(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->natural_size();
  }

  static base::TimeDelta timestamp(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->timestamp();
  }

  // TODO(hubbe): Return const ref when VideoFrame::ColorSpace()
  // returns const ref.
  static gfx::ColorSpace color_space(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->ColorSpace();
  }

  static const std::optional<gfx::HDRMetadata>& hdr_metadata(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->hdr_metadata();
  }

  static media::SharedImageFormatType shared_image_format_type(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->shared_image_format_type();
  }

  static const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->ycbcr_info();
  }

  static media::mojom::VideoFrameDataPtr data(
      const scoped_refptr<media::VideoFrame>& input);

  static const media::VideoFrameMetadata& metadata(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->metadata();
  }

  static bool Read(media::mojom::VideoFrameDataView input,
                   scoped_refptr<media::VideoFrame>* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_FRAME_MOJOM_TRAITS_H_
