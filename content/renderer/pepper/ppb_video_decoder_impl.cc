// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_video_decoder_impl.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/ppb_buffer_impl.h"
#include "content/renderer/pepper/ppb_graphics_3d_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "media/base/media_util.h"
#include "media/gpu/ipc/client/gpu_video_decode_accelerator_host.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"

using ppapi::TrackedCallback;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Graphics3D_API;

namespace {

// Convert PP_VideoDecoder_Profile to media::VideoCodecProfile.
media::VideoCodecProfile PPToMediaProfile(
    const PP_VideoDecoder_Profile pp_profile) {
  switch (pp_profile) {
    case PP_VIDEODECODER_H264PROFILE_NONE:
    // HACK: PPAPI contains a bogus "none" h264 profile that doesn't
    // correspond to anything in h.264; but a number of released chromium
    // versions silently promoted this to Baseline profile, so we retain that
    // behavior here.  Fall through.
    case PP_VIDEODECODER_H264PROFILE_BASELINE:
      return media::H264PROFILE_BASELINE;
    case PP_VIDEODECODER_H264PROFILE_MAIN:
      return media::H264PROFILE_MAIN;
    case PP_VIDEODECODER_H264PROFILE_EXTENDED:
      return media::H264PROFILE_EXTENDED;
    case PP_VIDEODECODER_H264PROFILE_HIGH:
      return media::H264PROFILE_HIGH;
    case PP_VIDEODECODER_H264PROFILE_HIGH10PROFILE:
      return media::H264PROFILE_HIGH10PROFILE;
    case PP_VIDEODECODER_H264PROFILE_HIGH422PROFILE:
      return media::H264PROFILE_HIGH422PROFILE;
    case PP_VIDEODECODER_H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return media::H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case PP_VIDEODECODER_H264PROFILE_SCALABLEBASELINE:
      return media::H264PROFILE_SCALABLEBASELINE;
    case PP_VIDEODECODER_H264PROFILE_SCALABLEHIGH:
      return media::H264PROFILE_SCALABLEHIGH;
    case PP_VIDEODECODER_H264PROFILE_STEREOHIGH:
      return media::H264PROFILE_STEREOHIGH;
    case PP_VIDEODECODER_H264PROFILE_MULTIVIEWHIGH:
      return media::H264PROFILE_MULTIVIEWHIGH;
    case PP_VIDEODECODER_VP8PROFILE_ANY:
      return media::VP8PROFILE_ANY;
    default:
      return media::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

PP_VideoDecodeError_Dev MediaToPPError(
    media::VideoDecodeAccelerator::Error error) {
  switch (error) {
    case media::VideoDecodeAccelerator::ILLEGAL_STATE:
      return PP_VIDEODECODERERROR_ILLEGAL_STATE;
    case media::VideoDecodeAccelerator::INVALID_ARGUMENT:
      return PP_VIDEODECODERERROR_INVALID_ARGUMENT;
    case media::VideoDecodeAccelerator::UNREADABLE_INPUT:
      return PP_VIDEODECODERERROR_UNREADABLE_INPUT;
    case media::VideoDecodeAccelerator::PLATFORM_FAILURE:
      return PP_VIDEODECODERERROR_PLATFORM_FAILURE;
    default:
      NOTREACHED();
      return PP_VIDEODECODERERROR_ILLEGAL_STATE;
  }
}

}  // namespace

namespace content {

PPB_VideoDecoder_Impl::PPB_VideoDecoder_Impl(PP_Instance instance)
    : PPB_VideoDecoder_Shared(instance), ppp_videodecoder_(nullptr) {}

PPB_VideoDecoder_Impl::~PPB_VideoDecoder_Impl() { Destroy(); }

// static
PP_Resource PPB_VideoDecoder_Impl::Create(PP_Instance instance,
                                          PP_Resource graphics_context,
                                          PP_VideoDecoder_Profile profile) {
  scoped_refptr<PPB_VideoDecoder_Impl> decoder(
      new PPB_VideoDecoder_Impl(instance));
  if (decoder->Init(graphics_context, profile))
    return decoder->GetReference();
  return 0;
}

bool PPB_VideoDecoder_Impl::Init(PP_Resource graphics_context,
                                 PP_VideoDecoder_Profile profile) {
  EnterResourceNoLock<PPB_Graphics3D_API> enter_context(graphics_context, true);
  if (enter_context.failed())
    return false;

  PPB_Graphics3D_Impl* graphics_3d =
      static_cast<PPB_Graphics3D_Impl*>(enter_context.object());

  gpu::CommandBufferProxyImpl* command_buffer =
      graphics_3d->GetCommandBufferProxy();
  if (!command_buffer)
    return false;

  InitCommon(graphics_context, graphics_3d->gles2_impl());
  FlushCommandBuffer();

  // This is not synchronous, but subsequent IPC messages will be buffered, so
  // it is okay to immediately send IPC messages.
  if (command_buffer->channel()) {
    decoder_.reset(new media::GpuVideoDecodeAcceleratorHost(command_buffer));
    media::VideoDecodeAccelerator::Config config(PPToMediaProfile(profile));
    config.supported_output_formats.assign(
        {media::PIXEL_FORMAT_XRGB, media::PIXEL_FORMAT_ARGB});
    return decoder_->Initialize(config, this);
  }
  return false;
}

const PPP_VideoDecoder_Dev* PPB_VideoDecoder_Impl::GetPPP() {
  if (!ppp_videodecoder_) {
    PluginModule* plugin_module =
        HostGlobals::Get()->GetInstance(pp_instance())->module();
    if (plugin_module) {
      ppp_videodecoder_ = static_cast<const PPP_VideoDecoder_Dev*>(
          plugin_module->GetPluginInterface(PPP_VIDEODECODER_DEV_INTERFACE));
    }
  }
  return ppp_videodecoder_;
}

int32_t PPB_VideoDecoder_Impl::Decode(
    const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
    scoped_refptr<TrackedCallback> callback) {
  if (!decoder_)
    return PP_ERROR_BADRESOURCE;

  EnterResourceNoLock<PPB_Buffer_API> enter(bitstream_buffer->data, true);
  if (enter.failed())
    return PP_ERROR_FAILED;

  PPB_Buffer_Impl* buffer = static_cast<PPB_Buffer_Impl*>(enter.object());
  DCHECK_GE(bitstream_buffer->id, 0);
  // TODO(crbug.com/844456): The shared memory buffer probably can be read-only,
  // but only after PPB_Buffer_Impl is updated to deal with that.
  media::BitstreamBuffer decode_buffer(bitstream_buffer->id,
                                       buffer->shared_memory().Duplicate(),
                                       bitstream_buffer->size);
  if (!SetBitstreamBufferCallback(bitstream_buffer->id, callback))
    return PP_ERROR_BADARGUMENT;

  FlushCommandBuffer();
  decoder_->Decode(std::move(decode_buffer));
  return PP_OK_COMPLETIONPENDING;
}

void PPB_VideoDecoder_Impl::AssignPictureBuffers(
    uint32_t no_of_buffers,
    const PP_PictureBuffer_Dev* buffers) {
  if (!decoder_)
    return;
  UMA_HISTOGRAM_COUNTS_100("Media.PepperVideoDecoderPictureCount",
                           no_of_buffers);

  std::vector<media::PictureBuffer> wrapped_buffers;
  for (uint32_t i = 0; i < no_of_buffers; i++) {
    PP_PictureBuffer_Dev in_buf = buffers[i];
    DCHECK_GE(in_buf.id, 0);
    media::PictureBuffer::TextureIds ids;
    ids.push_back(in_buf.texture_id);
    media::PictureBuffer buffer(
        in_buf.id, gfx::Size(in_buf.size.width, in_buf.size.height), ids);
    wrapped_buffers.push_back(buffer);
    UMA_HISTOGRAM_COUNTS_10000("Media.PepperVideoDecoderPictureHeight",
                               in_buf.size.height);
  }

  FlushCommandBuffer();
  decoder_->AssignPictureBuffers(wrapped_buffers);
}

void PPB_VideoDecoder_Impl::ReusePictureBuffer(int32_t picture_buffer_id) {
  if (!decoder_)
    return;

  FlushCommandBuffer();
  decoder_->ReusePictureBuffer(picture_buffer_id);
}

int32_t PPB_VideoDecoder_Impl::Flush(scoped_refptr<TrackedCallback> callback) {
  if (!decoder_)
    return PP_ERROR_BADRESOURCE;

  if (!SetFlushCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  decoder_->Flush();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_VideoDecoder_Impl::Reset(scoped_refptr<TrackedCallback> callback) {
  if (!decoder_)
    return PP_ERROR_BADRESOURCE;

  if (!SetResetCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  decoder_->Reset();
  return PP_OK_COMPLETIONPENDING;
}

void PPB_VideoDecoder_Impl::Destroy() {
  FlushCommandBuffer();

  decoder_.reset();
  ppp_videodecoder_ = nullptr;

  ::ppapi::PPB_VideoDecoder_Shared::Destroy();
}

void PPB_VideoDecoder_Impl::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    media::VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(1u, textures_per_buffer);
  if (!GetPPP())
    return;

  coded_size_ = dimensions;
  PP_Size out_dim = PP_MakeSize(dimensions.width(), dimensions.height());
  GetPPP()->ProvidePictureBuffers(pp_instance(), pp_resource(),
                                  requested_num_of_buffers, &out_dim,
                                  texture_target);
}

void PPB_VideoDecoder_Impl::PictureReady(const media::Picture& picture) {
  // So far picture.visible_rect is not used. If used, visible_rect should
  // be validated since it comes from GPU process and may not be trustworthy.
  DCHECK(RenderThreadImpl::current());
  if (!GetPPP())
    return;

  media::ReportPepperVideoDecoderOutputPictureCountHW(coded_size_.height());

  PP_Picture_Dev output;
  output.picture_buffer_id = picture.picture_buffer_id();
  output.bitstream_buffer_id = picture.bitstream_buffer_id();
  GetPPP()->PictureReady(pp_instance(), pp_resource(), &output);
}

void PPB_VideoDecoder_Impl::DismissPictureBuffer(int32_t picture_buffer_id) {
  DCHECK(RenderThreadImpl::current());
  if (!GetPPP())
    return;
  GetPPP()->DismissPictureBuffer(pp_instance(), pp_resource(),
                                 picture_buffer_id);
}

void PPB_VideoDecoder_Impl::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  DCHECK(RenderThreadImpl::current());
  if (!GetPPP())
    return;

  PP_VideoDecodeError_Dev pp_error = MediaToPPError(error);
  GetPPP()->NotifyError(pp_instance(), pp_resource(), pp_error);
  UMA_HISTOGRAM_ENUMERATION("Media.PepperVideoDecoderError", error,
                            media::VideoDecodeAccelerator::ERROR_MAX + 1);
}

void PPB_VideoDecoder_Impl::NotifyResetDone() {
  DCHECK(RenderThreadImpl::current());
  RunResetCallback(PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  DCHECK(RenderThreadImpl::current());
  RunBitstreamBufferCallback(bitstream_buffer_id, PP_OK);
}

void PPB_VideoDecoder_Impl::NotifyFlushDone() {
  DCHECK(RenderThreadImpl::current());
  RunFlushCallback(PP_OK);
}

}  // namespace content
