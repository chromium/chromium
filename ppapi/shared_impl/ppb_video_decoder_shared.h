// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_VIDEO_DECODER_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_VIDEO_DECODER_SHARED_H_

#include <stdint.h>

#include <map>

#include "base/compiler_specific.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_video_decoder_dev_api.h"

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace ppapi {

// Implements the logic to set and run callbacks for various video decoder
// events. Both the proxy and the renderer implementation share this code.
class PPAPI_SHARED_EXPORT PPB_VideoDecoder_Shared
    : public Resource,
      public thunk::PPB_VideoDecoder_Dev_API {
 public:
  explicit PPB_VideoDecoder_Shared(PP_Instance instance);
  explicit PPB_VideoDecoder_Shared(const HostResource& host_resource);

  PPB_VideoDecoder_Shared(const PPB_VideoDecoder_Shared&) = delete;
  PPB_VideoDecoder_Shared& operator=(const PPB_VideoDecoder_Shared&) = delete;

  ~PPB_VideoDecoder_Shared() override;

  // Resource overrides.
  thunk::PPB_VideoDecoder_Dev_API* AsPPB_VideoDecoder_Dev_API() override;

  // PPB_VideoDecoder_Dev_API implementation.
  void Destroy() override;

 protected:
  bool SetFlushCallback(scoped_refptr<TrackedCallback> callback);
  bool SetResetCallback(scoped_refptr<TrackedCallback> callback);
  bool SetBitstreamBufferCallback(int32_t bitstream_buffer_id,
                                  scoped_refptr<TrackedCallback> callback);

  void RunFlushCallback(int32_t result);
  void RunResetCallback(int32_t result);
  void RunBitstreamBufferCallback(int32_t bitstream_buffer_id, int32_t result);

  // Tell command buffer to process all commands it has received so far.
  void FlushCommandBuffer();

  // Initialize the underlying decoder.
  void InitCommon(PP_Resource graphics_context,
                  gpu::gles2::GLES2Implementation* gles2_impl);

 private:
  // Key: bitstream_buffer_id, value: callback to run when bitstream decode is
  // done.
  typedef std::map<int32_t, scoped_refptr<TrackedCallback>> CallbackById;

  scoped_refptr<TrackedCallback> flush_callback_;
  scoped_refptr<TrackedCallback> reset_callback_;
  CallbackById bitstream_buffer_callbacks_;

  // The resource ID of the underlying Graphics3D object being used.  Used only
  // for reference counting to keep it alive for the lifetime of |*this|.
  PP_Resource graphics_context_;

  // Reference to the GLES2Implementation owned by |graphics_context_|.
  // Graphics3D is guaranteed to be alive for the lifetime of this class.
  // In the out-of-process case, Graphics3D's gles2_impl() exists in the plugin
  // process only, so gles2_impl_ is NULL in that case.
  gpu::gles2::GLES2Implementation* gles2_impl_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_VIDEO_DECODER_SHARED_H_
