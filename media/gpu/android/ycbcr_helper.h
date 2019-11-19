// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_YCBCR_HELPER_H_
#define MEDIA_GPU_ANDROID_YCBCR_HELPER_H_

#include "base/optional.h"
#include "base/threading/sequence_bound.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Helper class to fetch YCbCrInfo for Vulkan from a CodecImage.
class MEDIA_GPU_EXPORT YCbCrHelper {
 public:
  using OptionalInfo = base::Optional<gpu::VulkanYCbCrInfo>;

  static base::SequenceBound<YCbCrHelper> Create(
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      SharedImageVideoProvider::GetStubCB get_stub_cb);

  virtual ~YCbCrHelper() = default;

  // Call |cb| with the YCbCrInfo (or nullopt, if we can't get it).  Will render
  // |codec_image_holder| to the front buffer if it hasn't successfully gotten
  // the YCbCrInfo on a previous call.  Otherwise, will return the cached
  // YCbCrInfo and leave |codec_image_holder| unmodified.  Once we call |cb|
  // with a non-nullopt YCbCrInfo, we will always return that same value; there
  // is no need to call us afterwards.
  //
  // While this API might seem to be out of its Vulkan mind, it's this
  // complicated to (a) prevent rendering frames out of order to the front
  // buffer, and (b) make it easy to handle the fact that sometimes, we just
  // can't get a YCbCrInfo from a CodecImage due to timeouts.
  virtual void GetYCbCrInfo(
      scoped_refptr<CodecImageHolder> codec_image_holder,
      base::OnceCallback<void(OptionalInfo ycbcr_info)> cb) = 0;

 protected:
  YCbCrHelper() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_YCBCR_HELPER_H_
