// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_INTERFACES_VIDEO_FRAME_STRUCT_TRAITS_H_
#define MEDIA_MOJO_INTERFACES_VIDEO_FRAME_STRUCT_TRAITS_H_

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "gpu/ipc/common/mailbox_holder_struct_traits.h"
#include "media/base/ipc/media_param_traits_macros.h"
#include "media/base/video_frame.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

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

  static media::mojom::VideoFrameDataPtr data(
      const scoped_refptr<media::VideoFrame>& input);

  static const base::Value& metadata(
      const scoped_refptr<media::VideoFrame>& input) {
    return input->metadata()->GetInternalValues();
  }

  static bool Read(media::mojom::VideoFrameDataView input,
                   scoped_refptr<media::VideoFrame>* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_INTERFACES_VIDEO_FRAME_STRUCT_TRAITS_H_
