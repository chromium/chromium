// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_video_encoder_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_math.h"
#include "build/build_config.h"
#include "content/common/pepper_file_util.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/renderer/ppapi_gfx_conversion.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/video_encoder_shim.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/video/video_encode_accelerator.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_buffer.h"

using ppapi::proxy::SerializedHandle;

namespace content {

namespace {

const uint32_t kDefaultNumberOfBitstreamBuffers = 4;

// TODO(llandwerlin): move following to media_conversion.cc/h?
media::VideoCodecProfile PP_ToMediaVideoProfile(PP_VideoProfile profile) {
  switch (profile) {
    case PP_VIDEOPROFILE_H264BASELINE:
      return media::H264PROFILE_BASELINE;
    case PP_VIDEOPROFILE_H264MAIN:
      return media::H264PROFILE_MAIN;
    case PP_VIDEOPROFILE_H264EXTENDED:
      return media::H264PROFILE_EXTENDED;
    case PP_VIDEOPROFILE_H264HIGH:
      return media::H264PROFILE_HIGH;
    case PP_VIDEOPROFILE_H264HIGH10PROFILE:
      return media::H264PROFILE_HIGH10PROFILE;
    case PP_VIDEOPROFILE_H264HIGH422PROFILE:
      return media::H264PROFILE_HIGH422PROFILE;
    case PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE:
      return media::H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case PP_VIDEOPROFILE_H264SCALABLEBASELINE:
      return media::H264PROFILE_SCALABLEBASELINE;
    case PP_VIDEOPROFILE_H264SCALABLEHIGH:
      return media::H264PROFILE_SCALABLEHIGH;
    case PP_VIDEOPROFILE_H264STEREOHIGH:
      return media::H264PROFILE_STEREOHIGH;
    case PP_VIDEOPROFILE_H264MULTIVIEWHIGH:
      return media::H264PROFILE_MULTIVIEWHIGH;
    case PP_VIDEOPROFILE_VP8_ANY:
      return media::VP8PROFILE_ANY;
    case PP_VIDEOPROFILE_VP9_ANY:
      return media::VP9PROFILE_PROFILE0;
    // No default case, to catch unhandled PP_VideoProfile values.
  }
  return media::VIDEO_CODEC_PROFILE_UNKNOWN;
}

PP_VideoProfile PP_FromMediaVideoProfile(media::VideoCodecProfile profile) {
  switch (profile) {
    case media::H264PROFILE_BASELINE:
      return PP_VIDEOPROFILE_H264BASELINE;
    case media::H264PROFILE_MAIN:
      return PP_VIDEOPROFILE_H264MAIN;
    case media::H264PROFILE_EXTENDED:
      return PP_VIDEOPROFILE_H264EXTENDED;
    case media::H264PROFILE_HIGH:
      return PP_VIDEOPROFILE_H264HIGH;
    case media::H264PROFILE_HIGH10PROFILE:
      return PP_VIDEOPROFILE_H264HIGH10PROFILE;
    case media::H264PROFILE_HIGH422PROFILE:
      return PP_VIDEOPROFILE_H264HIGH422PROFILE;
    case media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE;
    case media::H264PROFILE_SCALABLEBASELINE:
      return PP_VIDEOPROFILE_H264SCALABLEBASELINE;
    case media::H264PROFILE_SCALABLEHIGH:
      return PP_VIDEOPROFILE_H264SCALABLEHIGH;
    case media::H264PROFILE_STEREOHIGH:
      return PP_VIDEOPROFILE_H264STEREOHIGH;
    case media::H264PROFILE_MULTIVIEWHIGH:
      return PP_VIDEOPROFILE_H264MULTIVIEWHIGH;
    case media::VP8PROFILE_ANY:
      return PP_VIDEOPROFILE_VP8_ANY;
    case media::VP9PROFILE_PROFILE0:
      return PP_VIDEOPROFILE_VP9_ANY;
    default:
      NOTREACHED_IN_MIGRATION();
      return static_cast<PP_VideoProfile>(-1);
  }
}

media::VideoPixelFormat PP_ToMediaVideoFormat(PP_VideoFrame_Format format) {
  switch (format) {
    case PP_VIDEOFRAME_FORMAT_UNKNOWN:
      return media::PIXEL_FORMAT_UNKNOWN;
    case PP_VIDEOFRAME_FORMAT_YV12:
      return media::PIXEL_FORMAT_YV12;
    case PP_VIDEOFRAME_FORMAT_I420:
      return media::PIXEL_FORMAT_I420;
    case PP_VIDEOFRAME_FORMAT_BGRA:
      return media::PIXEL_FORMAT_UNKNOWN;
    // No default case, to catch unhandled PP_VideoFrame_Format values.
  }
  return media::PIXEL_FORMAT_UNKNOWN;
}

PP_VideoFrame_Format PP_FromMediaVideoFormat(media::VideoPixelFormat format) {
  switch (format) {
    case media::PIXEL_FORMAT_UNKNOWN:
      return PP_VIDEOFRAME_FORMAT_UNKNOWN;
    case media::PIXEL_FORMAT_YV12:
      return PP_VIDEOFRAME_FORMAT_YV12;
    case media::PIXEL_FORMAT_I420:
      return PP_VIDEOFRAME_FORMAT_I420;
    default:
      return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  }
}

PP_VideoProfileDescription PP_FromVideoEncodeAcceleratorSupportedProfile(
    media::VideoEncodeAccelerator::SupportedProfile profile) {
  PP_VideoProfileDescription pp_profile;
  pp_profile.profile = PP_FromMediaVideoProfile(profile.profile);
  pp_profile.max_resolution = PP_FromGfxSize(profile.max_resolution);
  pp_profile.max_framerate_numerator = profile.max_framerate_numerator;
  pp_profile.max_framerate_denominator = profile.max_framerate_denominator;
  pp_profile.hardware_accelerated = PP_FALSE;
  return pp_profile;
}

}  // namespace

PepperVideoEncoderHost::ShmBuffer::ShmBuffer(
    uint32_t id,
    base::UnsafeSharedMemoryRegion shm_region)
    : id(id), region(std::move(shm_region)), in_use(true) {
  DCHECK(region.IsValid());
  mapping = region.Map();
  DCHECK(mapping.IsValid());
}

PepperVideoEncoderHost::ShmBuffer::~ShmBuffer() {}

media::BitstreamBuffer PepperVideoEncoderHost::ShmBuffer::ToBitstreamBuffer() {
  DCHECK(region.IsValid());
  DCHECK(mapping.IsValid());
  return media::BitstreamBuffer(id, region.Duplicate(), mapping.size());
}

PepperVideoEncoderHost::PepperVideoEncoderHost(RendererPpapiHost* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      buffer_manager_(this),
      encoder_(new VideoEncoderShim(this)),
      initialized_(false),
      encoder_last_error_(PP_ERROR_FAILED),
      frame_count_(0),
      media_input_format_(media::PIXEL_FORMAT_UNKNOWN) {}

PepperVideoEncoderHost::~PepperVideoEncoderHost() {
  Close();
}

int32_t PepperVideoEncoderHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperVideoEncoderHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_VideoEncoder_GetSupportedProfiles,
        OnHostMsgGetSupportedProfiles)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoEncoder_Initialize,
                                      OnHostMsgInitialize)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_VideoEncoder_GetVideoFrames,
        OnHostMsgGetVideoFrames)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoEncoder_Encode,
                                      OnHostMsgEncode)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_VideoEncoder_RecycleBitstreamBuffer,
        OnHostMsgRecycleBitstreamBuffer)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_VideoEncoder_RequestEncodingParametersChange,
        OnHostMsgRequestEncodingParametersChange)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoEncoder_Close,
                                        OnHostMsgClose)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperVideoEncoderHost::OnGpuControlLostContext() {
#if DCHECK_IS_ON()
  // This should never occur more than once.
  DCHECK(!lost_context_);
  lost_context_ = true;
#endif
  NotifyPepperError(PP_ERROR_RESOURCE_FAILED);
}

void PepperVideoEncoderHost::OnGpuControlLostContextMaybeReentrant() {
  // No internal state to update on lost context.
}

void PepperVideoEncoderHost::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
  NOTIMPLEMENTED();
}

int32_t PepperVideoEncoderHost::OnHostMsgGetSupportedProfiles(
    ppapi::host::HostMessageContext* context) {
  std::vector<PP_VideoProfileDescription> pp_profiles;
  GetSupportedProfiles(&pp_profiles);

  host()->SendReply(
      context->MakeReplyMessageContext(),
      PpapiPluginMsg_VideoEncoder_GetSupportedProfilesReply(pp_profiles));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoEncoderHost::OnHostMsgInitialize(
    ppapi::host::HostMessageContext* context,
    PP_VideoFrame_Format input_format,
    const PP_Size& input_visible_size,
    PP_VideoProfile output_profile,
    uint32_t initial_bitrate,
    PP_HardwareAcceleration acceleration) {
  if (initialized_)
    return PP_ERROR_FAILED;

  media_input_format_ = PP_ToMediaVideoFormat(input_format);
  if (media_input_format_ == media::PIXEL_FORMAT_UNKNOWN)
    return PP_ERROR_BADARGUMENT;

  media::VideoCodecProfile media_profile =
      PP_ToMediaVideoProfile(output_profile);
  if (media_profile == media::VIDEO_CODEC_PROFILE_UNKNOWN)
    return PP_ERROR_BADARGUMENT;

  gfx::Size input_size(input_visible_size.width, input_visible_size.height);
  if (input_size.IsEmpty())
    return PP_ERROR_BADARGUMENT;

  if (acceleration == PP_HARDWAREACCELERATION_ONLY)
    return PP_ERROR_NOTSUPPORTED;

  initialize_reply_context_ = context->MakeReplyMessageContext();
  const media::VideoEncodeAccelerator::Config config(
      media_input_format_, input_size, media_profile,
      media::Bitrate::ConstantBitrate(initial_bitrate),
      media::VideoEncodeAccelerator::kDefaultFramerate,
      media::VideoEncodeAccelerator::Config::StorageType::kShmem,
      media::VideoEncodeAccelerator::Config::ContentType::kDisplay);
  if (encoder_->Initialize(config, this))
    return PP_OK_COMPLETIONPENDING;

  initialize_reply_context_ = ppapi::host::ReplyMessageContext();
  Close();
  return PP_ERROR_FAILED;
}

int32_t PepperVideoEncoderHost::OnHostMsgGetVideoFrames(
    ppapi::host::HostMessageContext* context) {
  if (encoder_last_error_)
    return encoder_last_error_;

  get_video_frames_reply_context_ = context->MakeReplyMessageContext();
  AllocateVideoFrames();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoEncoderHost::OnHostMsgEncode(
    ppapi::host::HostMessageContext* context,
    uint32_t frame_id,
    bool force_keyframe) {
  if (encoder_last_error_)
    return encoder_last_error_;

  if (frame_id >= frame_count_)
    return PP_ERROR_FAILED;

  encoder_->Encode(
      CreateVideoFrame(frame_id, context->MakeReplyMessageContext()),
      force_keyframe);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoEncoderHost::OnHostMsgRecycleBitstreamBuffer(
    ppapi::host::HostMessageContext* context,
    uint32_t buffer_id) {
  if (encoder_last_error_)
    return encoder_last_error_;

  if (buffer_id >= shm_buffers_.size() || shm_buffers_[buffer_id]->in_use)
    return PP_ERROR_FAILED;

  shm_buffers_[buffer_id]->in_use = true;
  encoder_->UseOutputBitstreamBuffer(
      shm_buffers_[buffer_id]->ToBitstreamBuffer());

  return PP_OK;
}

int32_t PepperVideoEncoderHost::OnHostMsgRequestEncodingParametersChange(
    ppapi::host::HostMessageContext* context,
    uint32_t bitrate,
    uint32_t framerate) {
  if (encoder_last_error_)
    return encoder_last_error_;

  encoder_->RequestEncodingParametersChange(
      media::Bitrate::ConstantBitrate(bitrate), framerate, std::nullopt);

  return PP_OK;
}

int32_t PepperVideoEncoderHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context) {
  encoder_last_error_ = PP_ERROR_FAILED;
  Close();

  return PP_OK;
}

void PepperVideoEncoderHost::RequireBitstreamBuffers(
    unsigned int frame_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK(RenderThreadImpl::current());
  // We assume RequireBitstreamBuffers is only called once.
  DCHECK(!initialized_);

  input_coded_size_ = input_coded_size;
  frame_count_ = frame_count;

  for (uint32_t i = 0; i < kDefaultNumberOfBitstreamBuffers; ++i) {
    base::UnsafeSharedMemoryRegion region =
        base::UnsafeSharedMemoryRegion::Create(output_buffer_size);
    if (!region.IsValid()) {
      shm_buffers_.clear();
      break;
    }

    shm_buffers_.push_back(std::make_unique<ShmBuffer>(i, std::move(region)));
  }

  // Feed buffers to the encoder.
  std::vector<SerializedHandle> handles;
  for (const auto& buffer : shm_buffers_) {
    encoder_->UseOutputBitstreamBuffer(buffer->ToBitstreamBuffer());
    handles.push_back(SerializedHandle(
        renderer_ppapi_host_->ShareUnsafeSharedMemoryRegionWithRemote(
            buffer->region)));
  }

  host()->SendUnsolicitedReplyWithHandles(
      pp_resource(),
      PpapiPluginMsg_VideoEncoder_BitstreamBuffers(
          static_cast<uint32_t>(output_buffer_size)),
      &handles);

  if (!initialized_) {
    // Tell the plugin that initialization has been successful if we
    // haven't already.
    initialized_ = true;
    encoder_last_error_ = PP_OK;
    host()->SendReply(initialize_reply_context_,
                      PpapiPluginMsg_VideoEncoder_InitializeReply(
                          frame_count, PP_FromGfxSize(input_coded_size)));
  }

  if (shm_buffers_.empty()) {
    NotifyPepperError(PP_ERROR_NOMEMORY);
    return;
  }

  // If the plugin already requested video frames, we can now answer
  // that request.
  if (get_video_frames_reply_context_.is_valid())
    AllocateVideoFrames();
}

void PepperVideoEncoderHost::BitstreamBufferReady(
    int32_t buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(shm_buffers_[buffer_id]->in_use);

  shm_buffers_[buffer_id]->in_use = false;
  // TODO: Pass timestamp. Tracked in crbug/613984.
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_VideoEncoder_BitstreamBufferReady(
          buffer_id, base::checked_cast<uint32_t>(metadata.payload_size_bytes),
          metadata.key_frame));
}

void PepperVideoEncoderHost::NotifyErrorStatus(
    const media::EncoderStatus& status) {
  DCHECK(RenderThreadImpl::current());
  CHECK(!status.is_ok());
  LOG(ERROR) << "NotifyErrorStatus() is called, code="
             << static_cast<int32_t>(status.code())
             << ", message=" << status.message();
  NotifyPepperError(PP_ERROR_RESOURCE_FAILED);
}

void PepperVideoEncoderHost::GetSupportedProfiles(
    std::vector<PP_VideoProfileDescription>* pp_profiles) {
  DCHECK(RenderThreadImpl::current());
  DCHECK(encoder_);

  const media::VideoEncodeAccelerator::SupportedProfiles media_profiles =
      encoder_->GetSupportedProfiles();
  for (const auto& media_profile : media_profiles) {
    pp_profiles->push_back(
        PP_FromVideoEncodeAcceleratorSupportedProfile(media_profile));
  }
}

void PepperVideoEncoderHost::Close() {
  DCHECK(RenderThreadImpl::current());

  encoder_ = nullptr;
  command_buffer_ = nullptr;
}

void PepperVideoEncoderHost::AllocateVideoFrames() {
  DCHECK(RenderThreadImpl::current());
  DCHECK(get_video_frames_reply_context_.is_valid());

  // Frames have already been allocated.
  if (buffer_manager_.number_of_buffers() > 0) {
    SendGetFramesErrorReply(PP_ERROR_FAILED);
    NOTREACHED_IN_MIGRATION();
    return;
  }

  base::CheckedNumeric<uint32_t> size =
      media::VideoFrame::AllocationSize(media_input_format_, input_coded_size_);
  uint32_t frame_size = size.ValueOrDie();
  size += sizeof(ppapi::MediaStreamBuffer::Video);
  uint32_t buffer_size = size.ValueOrDie();
  // Make each buffer 4 byte aligned.
  size += (4 - buffer_size % 4);
  uint32_t buffer_size_aligned = size.ValueOrDie();
  size *= frame_count_;
  uint32_t total_size = size.ValueOrDie();

  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(total_size);
  if (!region.IsValid() ||
      !buffer_manager_.SetBuffers(frame_count_, buffer_size_aligned,
                                  std::move(region), true)) {
    SendGetFramesErrorReply(PP_ERROR_NOMEMORY);
    return;
  }

  VLOG(4) << " frame_count=" << frame_count_ << " frame_size=" << frame_size
          << " buffer_size=" << buffer_size_aligned;

  for (int32_t i = 0; i < buffer_manager_.number_of_buffers(); ++i) {
    ppapi::MediaStreamBuffer::Video* buffer =
        &(buffer_manager_.GetBufferPointer(i)->video);
    buffer->header.size = buffer_manager_.buffer_size();
    buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
    buffer->format = PP_FromMediaVideoFormat(media_input_format_);
    buffer->size.width = input_coded_size_.width();
    buffer->size.height = input_coded_size_.height();
    buffer->data_size = frame_size;
  }

  DCHECK(get_video_frames_reply_context_.is_valid());
  get_video_frames_reply_context_.params.AppendHandle(SerializedHandle(
      renderer_ppapi_host_->ShareUnsafeSharedMemoryRegionWithRemote(
          buffer_manager_.region())));

  host()->SendReply(get_video_frames_reply_context_,
                    PpapiPluginMsg_VideoEncoder_GetVideoFramesReply(
                        frame_count_, buffer_size_aligned,
                        PP_FromGfxSize(input_coded_size_)));
  get_video_frames_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperVideoEncoderHost::SendGetFramesErrorReply(int32_t error) {
  get_video_frames_reply_context_.params.set_result(error);
  host()->SendReply(
      get_video_frames_reply_context_,
      PpapiPluginMsg_VideoEncoder_GetVideoFramesReply(0, 0, PP_MakeSize(0, 0)));
  get_video_frames_reply_context_ = ppapi::host::ReplyMessageContext();
}

scoped_refptr<media::VideoFrame> PepperVideoEncoderHost::CreateVideoFrame(
    uint32_t frame_id,
    const ppapi::host::ReplyMessageContext& reply_context) {
  DCHECK(RenderThreadImpl::current());

  ppapi::MediaStreamBuffer* buffer = buffer_manager_.GetBufferPointer(frame_id);
  DCHECK(buffer);
  // The shared memory handle does not need to be given to the video frame as
  // cross-process calls coordinate shared memory via a buffer index. See
  // ppapi/shared_impl/media_stream_buffer_manager.h for details.
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      media_input_format_, input_coded_size_, gfx::Rect(input_coded_size_),
      input_coded_size_, static_cast<uint8_t*>(buffer->video.data),
      buffer->video.data_size, base::TimeDelta());
  if (!frame) {
    NotifyPepperError(PP_ERROR_FAILED);
    return frame;
  }
  frame->AddDestructionObserver(
      base::BindOnce(&PepperVideoEncoderHost::FrameReleased,
                     weak_ptr_factory_.GetWeakPtr(), reply_context, frame_id));
  return frame;
}

void PepperVideoEncoderHost::FrameReleased(
    const ppapi::host::ReplyMessageContext& reply_context,
    uint32_t frame_id) {
  DCHECK(RenderThreadImpl::current());

  ppapi::host::ReplyMessageContext context = reply_context;
  context.params.set_result(encoder_last_error_);
  host()->SendReply(context, PpapiPluginMsg_VideoEncoder_EncodeReply(frame_id));
}

void PepperVideoEncoderHost::NotifyPepperError(int32_t error) {
  DCHECK(RenderThreadImpl::current());

  encoder_last_error_ = error;
  Close();
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_VideoEncoder_NotifyError(encoder_last_error_));
}

uint8_t* PepperVideoEncoderHost::ShmHandleToAddress(int32_t buffer_id) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_GE(buffer_id, 0);
  DCHECK_LT(buffer_id, static_cast<int32_t>(shm_buffers_.size()));
  return shm_buffers_[buffer_id]->mapping.GetMemoryAsSpan<uint8_t>().data();
}

}  // namespace content
