// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIDEO_CAPTURE_TARGET_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIDEO_CAPTURE_TARGET_MOJOM_TRAITS_H_

#include <utility>

#include "base/notreached.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "mojo/public/cpp/base/token_mojom_traits.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom-shared.h"
#include "services/viz/public/cpp/compositing/frame_sink_id_mojom_traits.h"
#include "services/viz/public/cpp/compositing/subtree_capture_id_mojom_traits.h"

namespace mojo {

template <>
struct UnionTraits<viz::mojom::VideoCaptureSubTargetDataView,
                   viz::VideoCaptureSubTarget> {
  static bool IsNull(const viz::VideoCaptureSubTarget& data) {
    return absl::holds_alternative<absl::monostate>(data);
  }

  static void SetToNull(viz::VideoCaptureSubTarget* data) {
    *data = viz::VideoCaptureSubTarget();
  }

  static const viz::RegionCaptureCropId& region_capture_crop_id(
      const viz::VideoCaptureSubTarget& data) {
    return absl::get<viz::RegionCaptureCropId>(data);
  }

  static viz::SubtreeCaptureId subtree_capture_id(
      const viz::VideoCaptureSubTarget& data) {
    return absl::get<viz::SubtreeCaptureId>(data);
  }

  using Tag = viz::mojom::VideoCaptureSubTargetDataView::Tag;
  static Tag GetTag(const viz::VideoCaptureSubTarget& data) {
    if (absl::holds_alternative<viz::RegionCaptureCropId>(data)) {
      return Tag::kRegionCaptureCropId;
    }
    DCHECK(absl::holds_alternative<viz::SubtreeCaptureId>(data));
    return Tag::kSubtreeCaptureId;
  }

  static bool Read(viz::mojom::VideoCaptureSubTargetDataView data,
                   viz::VideoCaptureSubTarget* out) {
    switch (data.tag()) {
      case Tag::kRegionCaptureCropId: {
        viz::RegionCaptureCropId crop_id;
        if (!data.ReadRegionCaptureCropId(&crop_id) || crop_id.is_zero())
          return false;

        *out = crop_id;
        return true;
      }
      case Tag::kSubtreeCaptureId: {
        viz::SubtreeCaptureId capture_id;
        if (!data.ReadSubtreeCaptureId(&capture_id) || !capture_id.is_valid())
          return false;

        *out = capture_id;
        return true;
      }
    }

    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct StructTraits<viz::mojom::VideoCaptureTargetDataView,
                    viz::VideoCaptureTarget> {
  static viz::FrameSinkId frame_sink_id(const viz::VideoCaptureTarget& input) {
    return input.frame_sink_id;
  }

  static const viz::VideoCaptureSubTarget& sub_target(
      const viz::VideoCaptureTarget& input) {
    return input.sub_target;
  }

  static bool Read(viz::mojom::VideoCaptureTargetDataView data,
                   viz::VideoCaptureTarget* out) {
    viz::VideoCaptureTarget target;
    if (!data.ReadSubTarget(&target.sub_target) ||
        !data.ReadFrameSinkId(&target.frame_sink_id))
      return false;

    if (!target.frame_sink_id.is_valid())
      return false;

    *out = std::move(target);
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_VIDEO_CAPTURE_TARGET_MOJOM_TRAITS_H_
