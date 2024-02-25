// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_FRAME_INFO_HELPER_H_
#define MEDIA_GPU_ANDROID_FRAME_INFO_HELPER_H_

#include <optional>

#include "base/task/sequenced_task_runner.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "media/gpu/media_gpu_export.h"

namespace media {
class CodecOutputBufferRenderer;

// Helper class to fetch YCbCrInfo for Vulkan from a CodecImage.
class MEDIA_GPU_EXPORT FrameInfoHelper {
 public:
  struct FrameInfo {
    FrameInfo();
    ~FrameInfo();

    FrameInfo(FrameInfo&&);
    FrameInfo(const FrameInfo&);
    FrameInfo& operator=(const FrameInfo&);

    gfx::Size coded_size;
    gfx::Rect visible_rect;
    std::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
  };

  using FrameInfoReadyCB =
      base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                              FrameInfo)>;

  static std::unique_ptr<FrameInfoHelper> Create(
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      SharedImageVideoProvider::GetStubCB get_stub_cb,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  virtual ~FrameInfoHelper() = default;

  // Call |cb| with the FrameInfo.  Will render |buffer_renderer| to the front
  // buffer if we don't have frame info cached. For Vulkan this also will
  // attempt to get YCbCrInfo and cache it.  If all necessary info is cached the
  // call will leave buffer_renderer intact and it can be rendered later.
  // Rendering can fail for reasons. This function will make best efforts to
  // fill FrameInfo which can be used to create VideoFrame.
  //
  // Callbacks will be executed and on callers sequence and guaranteed to be
  // called in order of GetFrameInfo calls. Callback can be called before this
  // function returns if all necessary info is available right away.
  //
  // While this API might seem to be out of its Vulkan mind, it's this
  // complicated to (a) prevent rendering frames out of order to the front
  // buffer, and (b) make it easy to handle the fact that sometimes, we just
  // can't get a YCbCrInfo from a CodecImage due to timeouts.
  virtual void GetFrameInfo(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      FrameInfoReadyCB callback) = 0;

  // Returns true if the FrameInfoHelper can't currently produce any more
  // frames.
  virtual bool IsStalled() const = 0;

 protected:
  FrameInfoHelper() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_FRAME_INFO_HELPER_H_
