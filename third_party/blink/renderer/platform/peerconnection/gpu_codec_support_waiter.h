// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_GPU_CODEC_SUPPORT_WAITER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_GPU_CODEC_SUPPORT_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {

class GpuCodecSupportWaiter {
 public:
  explicit GpuCodecSupportWaiter(
      media::GpuVideoAcceleratorFactories* gpu_factories);

  bool IsDecoderSupportKnown() const;
  bool IsEncoderSupportKnown() const;

  absl::optional<base::TimeDelta> wait_timeout_ms() const {
    return wait_timeout_ms_;
  }

 private:
  bool IsCodecSupportKnown(bool is_encoder) const;

  raw_ptr<media::GpuVideoAcceleratorFactories, ExperimentalRenderer>
      gpu_factories_;

  const absl::optional<base::TimeDelta> wait_timeout_ms_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_GPU_CODEC_SUPPORT_WAITER_H_
