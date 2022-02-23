// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vpx_rate_control.h"

#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

namespace media {

// Template method specialization for VP9.
// TODO(mcasas): Remove when VP8 also has a GetLoopfilterLevel() method.
template <>
int VPXRateControl<libvpx::VP9RateControlRtcConfig,
                   libvpx::VP9RateControlRTC,
                   libvpx::VP9RateControlRtcConfig>::GetLoopfilterLevel()
    const {
  return impl_->GetLoopfilterLevel();
}

}  // namespace media
