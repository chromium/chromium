// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_MOJOM_TRAITS_H_

#include "services/viz/public/mojom/compositing/frame_deadline.mojom.h"

#include "components/viz/common/quads/frame_deadline.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::FrameDeadlineDataView, viz::FrameDeadline> {
  static base::TimeTicks frame_start_time(const viz::FrameDeadline& input) {
    return input.frame_start_time();
  }

  static uint32_t deadline_in_frames(const viz::FrameDeadline& input) {
    return input.deadline_in_frames();
  }

  static base::TimeDelta frame_interval(const viz::FrameDeadline& input) {
    return input.frame_interval();
  }

  static bool use_default_lower_bound_deadline(
      const viz::FrameDeadline& input) {
    return input.use_default_lower_bound_deadline();
  }

  static bool Read(viz::mojom::FrameDeadlineDataView data,
                   viz::FrameDeadline* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_FRAME_DEADLINE_MOJOM_TRAITS_H_
