// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_video_decoder_host.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "content/common/pepper_file_util.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/ppapi_gfx_conversion.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/ppb_graphics_3d_impl.h"
#include "content/renderer/pepper/video_decoder_shim.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/video_decoder_constants.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"

using ppapi::proxy::SerializedHandle;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;

namespace content {

namespace {

bool UseSharedImagesForPepperVideo() {
  CHECK(base::CommandLine::ForCurrentProcess());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableUseSharedImagesForPepperVideo)) {
    LOG(WARNING) << "UseSharedImagesForPepperVideo: Disabled by policy";
    return false;
  }

  const bool enabled =
      base::FeatureList::IsEnabled(media::kUseSharedImagesForPepperVideo);
  LOG(WARNING) << "UseSharedImagesForPepperVideo: feature controlled: "
               << enabled;
  return enabled;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HardwareAccelerationBehavior : int {
  kOther = 0,

  // The PepperVideoDecoderHost used a hardware decoder backed by the legacy
  // VideoDecodeAccelerator path from beginning to end.
  kHardwareDecoderOnlyWithoutMojoVideoDecoder = 1,

  // The PepperVideoDecoderHost initialized (and possibly started using) a
  // hardware decoder backed by the legacy VideoDecodeAccelerator path but then
  // fell back to software decoding.
  kHardwareDecoderWithoutMojoVideoDecoderAndThenSoftwareDecoder = 2,

  // The PepperVideoDecoderHost used a hardware decoder backed by the newer
  // MojoVideoDecoder path from beginning to end.
  kHardwareDecoderOnlyWithMojoVideoDecoder = 3,

  // The PepperVideoDecoderHost initialized (and possibly started using) a
  // hardware decoder backed by the newer MojoVideoDecoder path but then fell
  // back to software decoding.
  kHardwareDecoderWithMojoVideoDecoderAndThenSoftwareDecoder = 4,

  // The PepperVideoDecoderHost used a software video decoder from beginning to
  // end.
  kSoftwareDecoderOnly = 5,

  kMaxValue = kSoftwareDecoderOnly
};

media::VideoCodecProfile PepperToMediaVideoProfile(PP_VideoProfile profile) {
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

}  // namespace

PepperVideoDecoderHost::PendingDecode::PendingDecode(
    int32_t decode_id,
    uint32_t shm_id,
    uint32_t size,
    const ppapi::host::ReplyMessageContext& reply_context)
    : decode_id(decode_id),
      shm_id(shm_id),
      size(size),
      reply_context(reply_context) {}

PepperVideoDecoderHost::PendingDecode::~PendingDecode() {}

PepperVideoDecoderHost::MappedBuffer::MappedBuffer(
    base::UnsafeSharedMemoryRegion region,
    base::WritableSharedMemoryMapping mapping)
    : region(std::move(region)), mapping(std::move(mapping)) {}

PepperVideoDecoderHost::MappedBuffer::~MappedBuffer() {}

PepperVideoDecoderHost::MappedBuffer::MappedBuffer(MappedBuffer&&) = default;
PepperVideoDecoderHost::MappedBuffer& PepperVideoDecoderHost::MappedBuffer::
operator=(MappedBuffer&&) = default;

PepperVideoDecoderHost::PepperVideoDecoderHost(RendererPpapiHost* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      use_shared_images_(UseSharedImagesForPepperVideo()) {}

PepperVideoDecoderHost::~PepperVideoDecoderHost() {
  if (use_shared_images_ && decoder_) {
    scoped_refptr<viz::RasterContextProvider> context_provider =
        decoder_->context_provider();
    // Destroy `decoder_`, so it will destroy all available shared images.
    decoder_->Destroy();

    // If video decoder was destroyed before plugin returned all shared images,
    // this is our last chance to destroy them.
    if (!shared_images_.empty()) {
      CHECK(context_provider);
      // Plugin's GLES2Interface and Renderer's RasterInterface are synchronized
      // by issued `ShallowFlushCHROMIUM` after each work. To synchronize with
      // SharedImageInterface we generate sync token here.
      gpu::SyncToken sync_token;
      context_provider->RasterInterface()->GenUnverifiedSyncTokenCHROMIUM(
          sync_token.GetData());

      auto* sii = context_provider->SharedImageInterface();

      for (const auto& shared_image : shared_images_) {
        // All assigned textures should have been destroyed by `decoder_`
        CHECK_NE(shared_image.second.state, PictureBufferState::ASSIGNED);
        sii->DestroySharedImage(sync_token, shared_image.first);
      }
    }
  }

  auto hw_behavior = HardwareAccelerationBehavior::kOther;
  if (software_fallback_used_) {
    if (mojo_video_decoder_path_initialized_) {
      hw_behavior = HardwareAccelerationBehavior::
          kHardwareDecoderWithMojoVideoDecoderAndThenSoftwareDecoder;
    } else {
      hw_behavior = HardwareAccelerationBehavior::kSoftwareDecoderOnly;
    }
  } else if (mojo_video_decoder_path_initialized_) {
    hw_behavior =
        HardwareAccelerationBehavior::kHardwareDecoderOnlyWithMojoVideoDecoder;
  }

  base::UmaHistogramEnumeration(
      "Media.PepperVideoDecoder.HardwareAccelerationBehavior", hw_behavior);
}

int32_t PepperVideoDecoderHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperVideoDecoderHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_Initialize,
                                      OnHostMsgInitialize)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_GetShm,
                                      OnHostMsgGetShm)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_Decode,
                                      OnHostMsgDecode)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_AssignTextures,
                                      OnHostMsgAssignTextures)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_VideoDecoder_RecyclePicture,
                                      OnHostMsgRecyclePicture)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_VideoDecoder_RecycleSharedImage,
        OnHostMsgRecycleSharedImage)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoDecoder_Flush,
                                        OnHostMsgFlush)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_VideoDecoder_Reset,
                                        OnHostMsgReset)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperVideoDecoderHost::OnHostMsgInitialize(
    ppapi::host::HostMessageContext* context,
    const ppapi::HostResource& graphics_context,
    PP_VideoProfile profile,
    PP_HardwareAcceleration acceleration,
    uint32_t min_picture_count) {
  if (initialized_)
    return PP_ERROR_FAILED;
  if (min_picture_count > ppapi::proxy::kMaximumPictureCount)
    return PP_ERROR_BADARGUMENT;

  EnterResourceNoLock<PPB_Graphics3D_API> enter_graphics(
      graphics_context.host_resource(), true);
  if (enter_graphics.failed())
    return PP_ERROR_FAILED;
  PPB_Graphics3D_Impl* graphics3d =
      static_cast<PPB_Graphics3D_Impl*>(enter_graphics.object());

  gpu::CommandBufferProxyImpl* command_buffer =
      graphics3d->GetCommandBufferProxy();
  if (!command_buffer)
    return PP_ERROR_FAILED;

  profile_ = PepperToMediaVideoProfile(profile);
  software_fallback_allowed_ = (acceleration != PP_HARDWAREACCELERATION_ONLY);

  min_picture_count_ = min_picture_count;

  if (acceleration != PP_HARDWAREACCELERATION_NONE) {
    uint32_t shim_texture_pool_size = media::limits::kMaxVideoFrames + 1;
    shim_texture_pool_size =
        std::max(shim_texture_pool_size, min_picture_count_);
    auto new_decoder =
        VideoDecoderShim::Create(this, shim_texture_pool_size,
                                 /*use_hw_decoder=*/true,
                                 /*use_shared_images=*/use_shared_images_);
    if (new_decoder && new_decoder->Initialize(profile_)) {
      decoder_ = std::move(new_decoder);
      initialized_ = true;
      mojo_video_decoder_path_initialized_ = true;
      context->reply_msg =
          PpapiPluginMsg_VideoDecoder_InitializeReply(use_shared_images_);
      return PP_OK;
    }

    decoder_.reset();
    if (acceleration == PP_HARDWAREACCELERATION_ONLY)
      return PP_ERROR_NOTSUPPORTED;
  }

#if BUILDFLAG(IS_ANDROID)
  return PP_ERROR_NOTSUPPORTED;
#else
  if (!TryFallbackToSoftwareDecoder())
    return PP_ERROR_FAILED;

  initialized_ = true;
  context->reply_msg =
      PpapiPluginMsg_VideoDecoder_InitializeReply(use_shared_images_);
  return PP_OK;
#endif
}

int32_t PepperVideoDecoderHost::OnHostMsgGetShm(
    ppapi::host::HostMessageContext* context,
    uint32_t shm_id,
    uint32_t shm_size) {
  if (!initialized_)
    return PP_ERROR_FAILED;

  // Make the buffers larger since we hope to reuse them.
  shm_size = std::max(
      shm_size,
      static_cast<uint32_t>(ppapi::proxy::kMinimumBitstreamBufferSize));
  if (shm_size > ppapi::proxy::kMaximumBitstreamBufferSize)
    return PP_ERROR_FAILED;

  if (shm_id >= ppapi::proxy::kMaximumPendingDecodes)
    return PP_ERROR_FAILED;
  // The shm_id must be inside or at the end of shm_buffers_.
  if (shm_id > shm_buffers_.size())
    return PP_ERROR_FAILED;
  // Reject an attempt to reallocate a busy shm buffer.
  if (shm_id < shm_buffers_.size() && shm_buffers_[shm_id].busy)
    return PP_ERROR_FAILED;

  auto shm = base::UnsafeSharedMemoryRegion::Create(shm_size);
  auto mapping = shm.Map();
  if (!shm.IsValid() || !mapping.IsValid())
    return PP_ERROR_FAILED;

  SerializedHandle handle(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          renderer_ppapi_host_->ShareUnsafeSharedMemoryRegionWithRemote(shm)));
  if (shm_id == shm_buffers_.size()) {
    shm_buffers_.emplace_back(std::move(shm), std::move(mapping));
  } else {
    // Note by the check above this buffer cannot be busy.
    shm_buffers_[shm_id] = MappedBuffer(std::move(shm), std::move(mapping));
  }

  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.AppendHandle(std::move(handle));
  host()->SendReply(reply_context,
                    PpapiPluginMsg_VideoDecoder_GetShmReply(shm_size));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoDecoderHost::OnHostMsgDecode(
    ppapi::host::HostMessageContext* context,
    uint32_t shm_id,
    uint32_t size,
    int32_t decode_id) {
  if (!initialized_)
    return PP_ERROR_FAILED;
  DCHECK(decoder_);
  // |shm_id| is just an index into shm_buffers_. Make sure it's in range.
  if (static_cast<size_t>(shm_id) >= shm_buffers_.size())
    return PP_ERROR_FAILED;
  // Reject an attempt to pass a busy buffer to the decoder again.
  if (shm_buffers_[shm_id].busy)
    return PP_ERROR_FAILED;
  // Reject non-unique decode_id values.
  if (GetPendingDecodeById(decode_id) != pending_decodes_.end())
    return PP_ERROR_FAILED;

  if (flush_reply_context_.is_valid() || reset_reply_context_.is_valid())
    return PP_ERROR_FAILED;

  pending_decodes_.push_back(PendingDecode(decode_id, shm_id, size,
                                           context->MakeReplyMessageContext()));

  shm_buffers_[shm_id].busy = true;
  decoder_->Decode(media::BitstreamBuffer(
      decode_id, shm_buffers_[shm_id].region.Duplicate(), size));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoDecoderHost::OnHostMsgAssignTextures(
    ppapi::host::HostMessageContext* context,
    const PP_Size& size,
    const std::vector<uint32_t>& texture_ids,
    const std::vector<gpu::Mailbox>& mailboxes) {
  if (!initialized_) {
    return PP_ERROR_FAILED;
  }

  if (texture_ids.size() != mailboxes.size()) {
    return PP_ERROR_FAILED;
  }

  if (use_shared_images_) {
    return PP_ERROR_FAILED;
  }

  DCHECK(decoder_);

  pending_texture_requests_--;
  DCHECK_GE(pending_texture_requests_, 0);

  // If |assign_textures_messages_to_dismiss_| is not 0 then decrement it and
  // dismiss the textures. This is necessary to ensure that after SW decoder
  // fallback the textures that were requested by the failed HW decoder are not
  // passed to the SW decoder.
  if (assign_textures_messages_to_dismiss_ > 0) {
    assign_textures_messages_to_dismiss_--;
    PictureBufferMap pictures_pending_dismission;
    for (auto& texture_id : texture_ids) {
      host()->SendUnsolicitedReply(
          pp_resource(),
          PpapiPluginMsg_VideoDecoder_DismissPicture(texture_id));
    }
    picture_buffer_map_.swap(pictures_pending_dismission);
    return PP_OK;
  }

  // Verify that the new texture IDs are unique and store them in
  // |new_textures|.
  PictureBufferMap new_textures;
  for (uint32_t texture_id : texture_ids) {
    if (base::Contains(picture_buffer_map_, texture_id) ||
        base::Contains(new_textures, texture_id)) {
      // Can't assign the same texture more than once.
      return PP_ERROR_BADARGUMENT;
    }
    new_textures.insert(
        std::make_pair(texture_id, PictureBufferState::ASSIGNED));
  }

  picture_buffer_map_.insert(new_textures.begin(), new_textures.end());

  std::vector<media::PictureBuffer> picture_buffers;
  for (uint32_t i = 0; i < texture_ids.size(); i++) {
    media::PictureBuffer::TextureIds ids;
    ids.push_back(texture_ids[i]);
    media::PictureBuffer buffer(
        texture_ids[i],  // Use the texture_id to identify the buffer.
        gfx::Size(size.width, size.height), ids);
    picture_buffers.push_back(buffer);
  }
  texture_mailboxes_ = mailboxes;
  decoder_->AssignPictureBuffers(picture_buffers);
  return PP_OK;
}

int32_t PepperVideoDecoderHost::OnHostMsgRecyclePicture(
    ppapi::host::HostMessageContext* context,
    uint32_t texture_id) {
  if (!initialized_) {
    return PP_ERROR_FAILED;
  }

  if (use_shared_images_) {
    return PP_ERROR_FAILED;
  }

  DCHECK(decoder_);

  auto it = picture_buffer_map_.find(texture_id);
  if (it == picture_buffer_map_.end()) {
    return PP_ERROR_BADARGUMENT;
  }

  switch (it->second) {
    case PictureBufferState::ASSIGNED:
      return PP_ERROR_BADARGUMENT;

    case PictureBufferState::IN_USE:
      it->second = PictureBufferState::ASSIGNED;
      decoder_->ReusePictureBuffer(texture_id);
      break;

    case PictureBufferState::DISMISSED:
      picture_buffer_map_.erase(it);
      // The texture was already dismissed by the decoder. Notify the plugin.
      host()->SendUnsolicitedReply(
          pp_resource(),
          PpapiPluginMsg_VideoDecoder_DismissPicture(texture_id));
      break;
  }

  return PP_OK;
}

int32_t PepperVideoDecoderHost::OnHostMsgRecycleSharedImage(
    ppapi::host::HostMessageContext* context,
    const gpu::Mailbox& mailbox) {
  if (!initialized_) {
    return PP_ERROR_FAILED;
  }

  if (!use_shared_images_) {
    return PP_ERROR_FAILED;
  }

  DCHECK(decoder_);

  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    return PP_ERROR_BADARGUMENT;
  }

  switch (it->second.state) {
    case PictureBufferState::ASSIGNED:
      return PP_ERROR_BADARGUMENT;

    case PictureBufferState::IN_USE:
      it->second.state = PictureBufferState::ASSIGNED;
      decoder_->ReuseSharedImage(mailbox, it->second.size);
      break;

    case PictureBufferState::DISMISSED:
      DestroySharedImageInternal(it);
      break;
  }

  return PP_OK;
}

int32_t PepperVideoDecoderHost::OnHostMsgFlush(
    ppapi::host::HostMessageContext* context) {
  if (!initialized_)
    return PP_ERROR_FAILED;
  DCHECK(decoder_);
  if (flush_reply_context_.is_valid() || reset_reply_context_.is_valid())
    return PP_ERROR_FAILED;

  flush_reply_context_ = context->MakeReplyMessageContext();
  decoder_->Flush();

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperVideoDecoderHost::OnHostMsgReset(
    ppapi::host::HostMessageContext* context) {
  if (!initialized_)
    return PP_ERROR_FAILED;
  DCHECK(decoder_);
  if (flush_reply_context_.is_valid() || reset_reply_context_.is_valid())
    return PP_ERROR_FAILED;

  reset_reply_context_ = context->MakeReplyMessageContext();
  decoder_->Reset();

  return PP_OK_COMPLETIONPENDING;
}

gpu::Mailbox PepperVideoDecoderHost::CreateSharedImage(gfx::Size size) {
  CHECK(use_shared_images_);
  CHECK(decoder_);
  const auto& context_provider = decoder_->context_provider();
  CHECK(context_provider);

  auto* sii = context_provider->SharedImageInterface();
  auto* rii = context_provider->RasterInterface();

  auto mailbox = sii->CreateSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace(),
      kTopLeft_GrSurfaceOrigin, kOpaque_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_RASTER,
      "PepperVideoDecoder", gpu::SurfaceHandle());

  // This SI will be used on raster interface later, to avoid plumbing
  // SyncTokens just for creation wait on it here.
  rii->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

  shared_images_.emplace(
      mailbox,
      SharedImage{.size = size, .state = PictureBufferState::ASSIGNED});
  return mailbox;
}

void PepperVideoDecoderHost::DestroySharedImage(const gpu::Mailbox& mailbox) {
  CHECK(use_shared_images_);
  auto it = shared_images_.find(mailbox);
  CHECK(it != shared_images_.end());

  // VideoDecoderShim tracks only assigned images.
  CHECK_EQ(it->second.state, PictureBufferState::ASSIGNED);
  DestroySharedImageInternal(it);
}

void PepperVideoDecoderHost::DestroySharedImageInternal(
    std::map<gpu::Mailbox, SharedImage>::iterator it) {
  CHECK(decoder_);
  const auto& context_provider = decoder_->context_provider();
  CHECK(context_provider);

  // Plugin's GLES2Interface and Renderer's RasterInterface are synchronized by
  // issued `ShallowFlushCHROMIUM` after each work. To synchronize with
  // SharedImageInterface we generate sync token here.
  gpu::SyncToken sync_token;
  context_provider->RasterInterface()->GenUnverifiedSyncTokenCHROMIUM(
      sync_token.GetData());

  auto* sii = context_provider->SharedImageInterface();
  sii->DestroySharedImage(sync_token, it->first);
  shared_images_.erase(it);
}

void PepperVideoDecoderHost::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    media::VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  CHECK(!use_shared_images_);

  DCHECK_EQ(1u, textures_per_buffer);
  coded_size_ = dimensions;
  pending_texture_requests_++;
  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_VideoDecoder_RequestTextures(
                         std::max(min_picture_count_, requested_num_of_buffers),
                         PP_MakeSize(dimensions.width(), dimensions.height()),
                         texture_target));
}

void PepperVideoDecoderHost::PictureReady(const media::Picture& picture) {
  CHECK(!use_shared_images_);
  auto it = picture_buffer_map_.find(picture.picture_buffer_id());
  DCHECK(it != picture_buffer_map_.end());
  // VDA might send the same picture multiple times in VP9 video. However the
  // Pepper client might not able to handle it. Therefore we just catch it here.
  // https://crbug.com/755887
  CHECK(it->second == PictureBufferState::ASSIGNED);
  it->second = PictureBufferState::IN_USE;

  // Don't bother validating the visible rect, since the plugin process is less
  // trusted than the gpu process.
  PP_Rect visible_rect = PP_FromGfxRect(picture.visible_rect());
  host()->SendUnsolicitedReply(pp_resource(),
                               PpapiPluginMsg_VideoDecoder_PictureReady(
                                   picture.bitstream_buffer_id(),
                                   picture.picture_buffer_id(), visible_rect));
}

void PepperVideoDecoderHost::SharedImageReady(int32_t bitstream_id,
                                              const gpu::Mailbox& mailbox,
                                              gfx::Size size,
                                              const gfx::Rect& visible_rect) {
  CHECK(use_shared_images_);
  auto it = shared_images_.find(mailbox);
  CHECK(it != shared_images_.end());
  CHECK_EQ(it->second.state, PictureBufferState::ASSIGNED);
  it->second.state = PictureBufferState::IN_USE;

  host()->SendUnsolicitedReply(pp_resource(),
                               PpapiPluginMsg_VideoDecoder_SharedImageReady(
                                   bitstream_id, mailbox, PP_FromGfxSize(size),
                                   PP_FromGfxRect(visible_rect)));
}

void PepperVideoDecoderHost::DismissPictureBuffer(int32_t picture_buffer_id) {
  CHECK(!use_shared_images_);

  auto it = picture_buffer_map_.find(picture_buffer_id);
  DCHECK(it != picture_buffer_map_.end());

  // If the texture is still used by the plugin keep it until the plugin
  // recycles it.
  if (it->second == PictureBufferState::IN_USE) {
    it->second = PictureBufferState::DISMISSED;
    return;
  }

  DCHECK(it->second == PictureBufferState::ASSIGNED);
  picture_buffer_map_.erase(it);
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_VideoDecoder_DismissPicture(picture_buffer_id));
}

void PepperVideoDecoderHost::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  auto it = GetPendingDecodeById(bitstream_buffer_id);
  if (it == pending_decodes_.end()) {
    NOTREACHED();
    return;
  }
  host()->SendReply(it->reply_context,
                    PpapiPluginMsg_VideoDecoder_DecodeReply(it->shm_id));
  shm_buffers_[it->shm_id].busy = false;
  pending_decodes_.erase(it);
}

void PepperVideoDecoderHost::NotifyFlushDone() {
  DCHECK(pending_decodes_.empty());
  host()->SendReply(flush_reply_context_,
                    PpapiPluginMsg_VideoDecoder_FlushReply());
  flush_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperVideoDecoderHost::NotifyResetDone() {
  DCHECK(pending_decodes_.empty());
  host()->SendReply(reset_reply_context_,
                    PpapiPluginMsg_VideoDecoder_ResetReply());
  reset_reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperVideoDecoderHost::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  int32_t pp_error = PP_ERROR_FAILED;
  switch (error) {
    case media::VideoDecodeAccelerator::UNREADABLE_INPUT:
      pp_error = PP_ERROR_MALFORMED_INPUT;
      break;
    case media::VideoDecodeAccelerator::ILLEGAL_STATE:
    case media::VideoDecodeAccelerator::INVALID_ARGUMENT:
    case media::VideoDecodeAccelerator::PLATFORM_FAILURE:
      pp_error = PP_ERROR_RESOURCE_FAILED;
      break;
    // No default case, to catch unhandled enum values.
  }

  // Try to initialize software decoder and use it instead.
  if (!software_fallback_used_ && software_fallback_allowed_) {
    VLOG(0)
        << "Hardware decoder has returned an error. Trying Software decoder.";
    if (TryFallbackToSoftwareDecoder())
      return;
  }

  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_VideoDecoder_NotifyError(pp_error));
}

const uint8_t* PepperVideoDecoderHost::DecodeIdToAddress(uint32_t decode_id) {
  PendingDecodeList::const_iterator it = GetPendingDecodeById(decode_id);
  DCHECK(it != pending_decodes_.end());
  uint32_t shm_id = it->shm_id;
  return static_cast<uint8_t*>(shm_buffers_[shm_id].mapping.memory());
}

bool PepperVideoDecoderHost::TryFallbackToSoftwareDecoder() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  DCHECK(!software_fallback_used_ && software_fallback_allowed_);

  uint32_t shim_texture_pool_size = media::limits::kMaxVideoFrames + 1;
  shim_texture_pool_size = std::max(shim_texture_pool_size,
                                    min_picture_count_);
  std::unique_ptr<VideoDecoderShim> new_decoder(VideoDecoderShim::Create(
      this, shim_texture_pool_size, /*use_hw_decoder=*/false,
      /*use_shared_images=*/use_shared_images_));
  if (!new_decoder || !new_decoder->Initialize(profile_)) {
    return false;
  }

  software_fallback_used_ = true;

  if (decoder_) {
    decoder_->Destroy();
    decoder_.reset();
  }
  decoder_ = std::move(new_decoder);

  if (use_shared_images_) {
    CHECK(picture_buffer_map_.empty());
    for (auto& shared_image : shared_images_) {
      // All ASSIGNED images were deleted by decoder. And there shouldn't be any
      // DISMISSED images yet, because it's set only in this function and this
      // point can only be reached once.

      CHECK_EQ(shared_image.second.state, PictureBufferState::IN_USE);
      // Mark as dismissed and delete once plug-in returns them.
      shared_image.second.state = PictureBufferState::DISMISSED;
    }
  } else {
    // Dismiss all assigned pictures and mark all pictures in use as DISMISSED.
    PictureBufferMap pictures_pending_dismission;
    for (auto& picture : picture_buffer_map_) {
      if (picture.second == PictureBufferState::ASSIGNED) {
        host()->SendUnsolicitedReply(
            pp_resource(),
            PpapiPluginMsg_VideoDecoder_DismissPicture(picture.first));
      } else {
        pictures_pending_dismission.insert(
            std::make_pair(picture.first, PictureBufferState::DISMISSED));
      }
    }
    picture_buffer_map_.swap(pictures_pending_dismission);
  }

  // Dismiss all outstanding texture requests.
  DCHECK_EQ(assign_textures_messages_to_dismiss_, 0);
  assign_textures_messages_to_dismiss_ = pending_texture_requests_;

  // If there was a pending Reset() it can be finished now.
  if (reset_reply_context_.is_valid()) {
    while (!pending_decodes_.empty()) {
      const PendingDecode& decode = pending_decodes_.front();
      host()->SendReply(decode.reply_context,
                        PpapiPluginMsg_VideoDecoder_DecodeReply(decode.shm_id));
      DCHECK(shm_buffers_[decode.shm_id].busy);
      shm_buffers_[decode.shm_id].busy = false;
      pending_decodes_.pop_front();
    }
    NotifyResetDone();
  }

  // Resubmit all pending decodes.
  for (const PendingDecode& decode : pending_decodes_) {
    DCHECK(shm_buffers_[decode.shm_id].busy);
    decoder_->Decode(media::BitstreamBuffer(
        decode.decode_id, shm_buffers_[decode.shm_id].region.Duplicate(),
        decode.size));
  }

  // Flush the new decoder if Flush() was pending.
  if (flush_reply_context_.is_valid())
    decoder_->Flush();

  return true;
#endif
}

PepperVideoDecoderHost::PendingDecodeList::iterator
PepperVideoDecoderHost::GetPendingDecodeById(int32_t decode_id) {
  return base::ranges::find(pending_decodes_, decode_id,
                            &PendingDecode::decode_id);
}

}  // namespace content
