// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_VIDEO_DECODER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_VIDEO_DECODER_IMPL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppb_video_decoder_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_video_decoder_dev_api.h"

struct PP_PictureBuffer_Dev;
struct PP_VideoBitstreamBuffer_Dev;

namespace content {

class PPB_VideoDecoder_Impl : public ppapi::PPB_VideoDecoder_Shared,
                              public media::VideoDecodeAccelerator::Client {
 public:
  // See PPB_VideoDecoder_Dev::Create.  Returns 0 on failure to create &
  // initialize.
  static PP_Resource Create(PP_Instance instance,
                            PP_Resource graphics_context,
                            PP_VideoDecoder_Profile profile);

  // PPB_VideoDecoder_Dev_API implementation.
  int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                 scoped_refptr<ppapi::TrackedCallback> callback) override;
  void AssignPictureBuffers(uint32_t no_of_buffers,
                            const PP_PictureBuffer_Dev* buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  int32_t Flush(scoped_refptr<ppapi::TrackedCallback> callback) override;
  int32_t Reset(scoped_refptr<ppapi::TrackedCallback> callback) override;
  void Destroy() override;

  // media::VideoDecodeAccelerator::Client implementation.
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             media::VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& dimensions,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void PictureReady(const media::Picture& picture) override;
  void NotifyError(media::VideoDecodeAccelerator::Error error) override;
  void NotifyFlushDone() override;
  void NotifyEndOfBitstreamBuffer(int32_t buffer_id) override;
  void NotifyResetDone() override;

 private:
  ~PPB_VideoDecoder_Impl() override;

  explicit PPB_VideoDecoder_Impl(PP_Instance instance);
  bool Init(PP_Resource graphics_context,
            PP_VideoDecoder_Profile profile);
  // Returns the associated PPP_VideoDecoder_Dev interface to use when
  // making calls on the plugin. This fetches the interface lazily. For
  // out-of-process plugins, this means a synchronous message to the plugin,
  // so it's important to never call this in response to a synchronous
  // plugin->renderer message (such as the Create message).
  const PPP_VideoDecoder_Dev* GetPPP();

  // This is NULL before initialization, and after destruction.
  // Holds a GpuVideoDecodeAcceleratorHost.
  std::unique_ptr<media::VideoDecodeAccelerator> decoder_;

  // Used for UMA stats; not frame-accurate.
  gfx::Size coded_size_;

  // The interface to use when making calls on the plugin. For the most part,
  // methods should not use this directly but should call GetPPP() instead.
  const PPP_VideoDecoder_Dev* ppp_videodecoder_;

  DISALLOW_COPY_AND_ASSIGN(PPB_VideoDecoder_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_VIDEO_DECODER_IMPL_H_
