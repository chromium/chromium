// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/video_decoder_resource.h"

#include <utility>

#include "base/functional/bind.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/proxy/video_decoder_constants.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_VideoDecoder_API;

namespace ppapi {
namespace proxy {

VideoDecoderResource::ShmBuffer::ShmBuffer(
    base::UnsafeSharedMemoryRegion region,
    uint32_t shm_id)
    : region(std::move(region)), shm_id(shm_id) {
  mapping = this->region.Map();
  if (mapping.IsValid())
    addr = mapping.memory();
}

VideoDecoderResource::ShmBuffer::~ShmBuffer() {
}

VideoDecoderResource::VideoDecoderResource(Connection connection,
                                           PP_Instance instance)
    : PluginResource(connection, instance),
      num_decodes_(0),
      min_picture_count_(0),
      get_picture_(NULL),
      get_picture_0_1_(NULL),
      gles2_impl_(NULL),
      initialized_(false),
      testing_(false),
      // Set |decoder_last_error_| to PP_OK after successful initialization.
      // This makes error checking a little more concise, since we can check
      // that the decoder has been initialized and hasn't returned an error by
      // just testing |decoder_last_error_|.
      decoder_last_error_(PP_ERROR_FAILED) {
  // Clear the decode_ids_ array.
  memset(decode_ids_, 0, sizeof(decode_ids_));
  SendCreate(RENDERER, PpapiHostMsg_VideoDecoder_Create());
}

VideoDecoderResource::~VideoDecoderResource() {
  // Destroy any textures which haven't been dismissed.
  if (initialized_) {
    if (!testing_) {
      CHECK(gles2_impl_);
      for (const auto& shared_image : used_shared_images_) {
        gles2_impl_->EndSharedImageAccessDirectCHROMIUM(shared_image.first);
        gles2_impl_->DeleteTextures(1, &shared_image.first);
      }

      gles2_impl_->ShallowFlushCHROMIUM();
    }
  }
}

PPB_VideoDecoder_API* VideoDecoderResource::AsPPB_VideoDecoder_API() {
  return this;
}

int32_t VideoDecoderResource::Initialize0_1(
    PP_Resource graphics_context,
    PP_VideoProfile profile,
    PP_Bool allow_software_fallback,
    scoped_refptr<TrackedCallback> callback) {
  return Initialize(graphics_context,
                    profile,
                    allow_software_fallback
                        ? PP_HARDWAREACCELERATION_WITHFALLBACK
                        : PP_HARDWAREACCELERATION_ONLY,
                    0,
                    callback);
}

int32_t VideoDecoderResource::Initialize0_2(
    PP_Resource graphics_context,
    PP_VideoProfile profile,
    PP_HardwareAcceleration acceleration,
    scoped_refptr<TrackedCallback> callback) {
  return Initialize(graphics_context,
                    profile,
                    acceleration,
                    0,
                    callback);
}

int32_t VideoDecoderResource::Initialize(
    PP_Resource graphics_context,
    PP_VideoProfile profile,
    PP_HardwareAcceleration acceleration,
    uint32_t min_picture_count,
    scoped_refptr<TrackedCallback> callback) {
  if (initialized_)
    return PP_ERROR_FAILED;
  if (profile < 0 || profile > PP_VIDEOPROFILE_MAX)
    return PP_ERROR_BADARGUMENT;
  if (min_picture_count > kMaximumPictureCount)
    return PP_ERROR_BADARGUMENT;
  if (initialize_callback_.get())
    return PP_ERROR_INPROGRESS;
  if (!graphics_context)
    return PP_ERROR_BADRESOURCE;

  min_picture_count_ = min_picture_count;

  HostResource host_resource;
  if (!testing_) {
    // Create a new Graphics3D resource that can create texture resources to
    // share with the plugin. We can't use the plugin's Graphics3D, since we
    // create textures on a proxy thread, and would interfere with the plugin.
    thunk::EnterResourceCreationNoLock enter_create(pp_instance());
    if (enter_create.failed())
      return PP_ERROR_FAILED;
    int32_t attrib_list[] = {PP_GRAPHICS3DATTRIB_NONE};
    graphics3d_ =
        ScopedPPResource(ScopedPPResource::PassRef(),
                         enter_create.functions()->CreateGraphics3D(
                             pp_instance(), graphics_context, attrib_list));
    EnterResourceNoLock<PPB_Graphics3D_API> enter_graphics(graphics3d_.get(),
                                                           false);
    if (enter_graphics.failed())
      return PP_ERROR_BADRESOURCE;

    PPB_Graphics3D_Shared* ppb_graphics3d_shared =
        static_cast<PPB_Graphics3D_Shared*>(enter_graphics.object());
    gles2_impl_ = ppb_graphics3d_shared->gles2_impl();
    host_resource = ppb_graphics3d_shared->host_resource();
  }

  initialize_callback_ = callback;

  Call<PpapiPluginMsg_VideoDecoder_InitializeReply>(
      RENDERER,
      PpapiHostMsg_VideoDecoder_Initialize(host_resource, profile, acceleration,
                                           min_picture_count),
      base::BindOnce(&VideoDecoderResource::OnPluginMsgInitializeComplete,
                     this));

  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoDecoderResource::Decode(uint32_t decode_id,
                                     uint32_t size,
                                     const void* buffer,
                                     scoped_refptr<TrackedCallback> callback) {
  if (decoder_last_error_)
    return decoder_last_error_;
  if (flush_callback_.get() || reset_callback_.get())
    return PP_ERROR_FAILED;
  if (decode_callback_.get())
    return PP_ERROR_INPROGRESS;
  if (size > kMaximumBitstreamBufferSize)
    return PP_ERROR_NOMEMORY;

  // If we allow the plugin to call Decode again, we must have somewhere to
  // copy their buffer.
  DCHECK(!available_shm_buffers_.empty() ||
         shm_buffers_.size() < kMaximumPendingDecodes);

  // Count up, wrapping back to 0 before overflowing.
  int32_t uid = ++num_decodes_;
  if (uid == std::numeric_limits<int32_t>::max())
    num_decodes_ = 0;

  // Save decode_id in a ring buffer. The ring buffer is sized to store
  // decode_id for the maximum picture delay.
  decode_ids_[uid % kMaximumPictureDelay] = decode_id;

  if (available_shm_buffers_.empty() ||
      available_shm_buffers_.back()->mapping.size() < size) {
    uint32_t shm_id;
    if (shm_buffers_.size() < kMaximumPendingDecodes) {
      // Signal the host to create a new shm buffer by passing an index outside
      // the legal range.
      shm_id = static_cast<uint32_t>(shm_buffers_.size());
    } else {
      // Signal the host to grow a buffer by passing a legal index. Choose the
      // last available shm buffer for simplicity.
      shm_id = available_shm_buffers_.back()->shm_id;
      available_shm_buffers_.pop_back();
    }

    // Synchronously get shared memory. Use GenericSyncCall so we can get the
    // reply params, which contain the handle.
    uint32_t shm_size = 0;
    IPC::Message reply;
    ResourceMessageReplyParams reply_params;
    int32_t result =
        GenericSyncCall(RENDERER,
                        PpapiHostMsg_VideoDecoder_GetShm(shm_id, size),
                        &reply,
                        &reply_params);
    if (result != PP_OK)
      return PP_ERROR_FAILED;
    if (!UnpackMessage<PpapiPluginMsg_VideoDecoder_GetShmReply>(reply,
                                                                &shm_size))
      return PP_ERROR_FAILED;
    base::UnsafeSharedMemoryRegion shm_region;
    if (!reply_params.TakeUnsafeSharedMemoryRegionAtIndex(0, &shm_region) ||
        !shm_region.IsValid() || shm_region.GetSize() != shm_size)
      return PP_ERROR_NOMEMORY;
    std::unique_ptr<ShmBuffer> shm_buffer(
        new ShmBuffer(std::move(shm_region), shm_id));
    if (!shm_buffer->addr)
      return PP_ERROR_NOMEMORY;

    available_shm_buffers_.push_back(shm_buffer.get());
    if (shm_buffers_.size() < kMaximumPendingDecodes)
      shm_buffers_.push_back(std::move(shm_buffer));
    else
      shm_buffers_[shm_id] = std::move(shm_buffer);
  }

  // At this point we should have shared memory to hold the plugin's buffer.
  DCHECK(!available_shm_buffers_.empty() &&
         available_shm_buffers_.back()->mapping.size() >= size);

  ShmBuffer* shm_buffer = available_shm_buffers_.back();
  available_shm_buffers_.pop_back();
  memcpy(shm_buffer->addr, buffer, size);

  Call<PpapiPluginMsg_VideoDecoder_DecodeReply>(
      RENDERER, PpapiHostMsg_VideoDecoder_Decode(shm_buffer->shm_id, size, uid),
      base::BindOnce(&VideoDecoderResource::OnPluginMsgDecodeComplete, this));

  // If we have another free buffer, or we can still create new buffers, let
  // the plugin call Decode again.
  if (!available_shm_buffers_.empty() ||
      shm_buffers_.size() < kMaximumPendingDecodes)
    return PP_OK;

  // All buffers are busy and we can't create more. Delay completion until a
  // buffer is available.
  decode_callback_ = callback;
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoDecoderResource::GetPicture0_1(
    PP_VideoPicture_0_1* picture,
    scoped_refptr<TrackedCallback> callback) {
  get_picture_0_1_ = picture;
  return GetPicture(NULL, callback);
}

int32_t VideoDecoderResource::GetPicture(
    PP_VideoPicture* picture,
    scoped_refptr<TrackedCallback> callback) {
  if (!initialized_) {
    return PP_ERROR_FAILED;
  }
  if (decoder_last_error_) {
    return decoder_last_error_;
  }
  if (reset_callback_.get()) {
    return PP_ERROR_FAILED;
  }
  if (get_picture_callback_.get()) {
    return PP_ERROR_INPROGRESS;
  }

  get_picture_ = picture;

  // If the next shared image is ready, return it synchronously.
  if (!received_shared_images_.empty()) {
    WriteNextSharedImage();
    return PP_OK;
  }

  get_picture_callback_ = callback;

  return PP_OK_COMPLETIONPENDING;
}

void VideoDecoderResource::RecyclePicture(const PP_VideoPicture* picture) {
  if (!initialized_) {
    return;
  }
  if (decoder_last_error_) {
    return;
  }

  auto it = used_shared_images_.find(picture->texture_id);
  if (it != used_shared_images_.end()) {
    gpu::Mailbox mailbox = it->second.mailbox;

    if (!testing_) {
      CHECK(gles2_impl_);
      gles2_impl_->EndSharedImageAccessDirectCHROMIUM(picture->texture_id);
      gles2_impl_->DeleteTextures(1, &picture->texture_id);
      gles2_impl_->ShallowFlushCHROMIUM();
    }

    used_shared_images_.erase(it);

    Post(RENDERER, PpapiHostMsg_VideoDecoder_RecycleSharedImage(mailbox));
  }
}

int32_t VideoDecoderResource::Flush(scoped_refptr<TrackedCallback> callback) {
  if (decoder_last_error_)
    return decoder_last_error_;
  if (reset_callback_.get())
    return PP_ERROR_FAILED;
  if (flush_callback_.get())
    return PP_ERROR_INPROGRESS;
  flush_callback_ = callback;

  Call<PpapiPluginMsg_VideoDecoder_FlushReply>(
      RENDERER, PpapiHostMsg_VideoDecoder_Flush(),
      base::BindOnce(&VideoDecoderResource::OnPluginMsgFlushComplete, this));

  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoDecoderResource::Reset(scoped_refptr<TrackedCallback> callback) {
  if (decoder_last_error_)
    return decoder_last_error_;
  if (flush_callback_.get())
    return PP_ERROR_FAILED;
  if (reset_callback_.get())
    return PP_ERROR_INPROGRESS;
  reset_callback_ = callback;

  // Cause any pending Decode or GetPicture callbacks to abort after we return,
  // to avoid reentering the plugin.
  if (TrackedCallback::IsPending(decode_callback_))
    decode_callback_->PostAbort();
  decode_callback_.reset();
  if (TrackedCallback::IsPending(get_picture_callback_))
    get_picture_callback_->PostAbort();
  get_picture_callback_.reset();
  Call<PpapiPluginMsg_VideoDecoder_ResetReply>(
      RENDERER, PpapiHostMsg_VideoDecoder_Reset(),
      base::BindOnce(&VideoDecoderResource::OnPluginMsgResetComplete, this));

  return PP_OK_COMPLETIONPENDING;
}

void VideoDecoderResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(VideoDecoderResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoDecoder_SharedImageReady,
        OnPluginMsgSharedImageReady)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoDecoder_NotifyError, OnPluginMsgNotifyError)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(
        PluginResource::OnReplyReceived(params, msg))
  PPAPI_END_MESSAGE_MAP()
}

void VideoDecoderResource::SetForTest() {
  testing_ = true;
}

void VideoDecoderResource::OnPluginMsgSharedImageReady(
    const ResourceMessageReplyParams& params,
    int32_t decode_id,
    const gpu::Mailbox& mailbox,
    const PP_Size& size,
    const PP_Rect& visible_rect) {
  received_shared_images_.push(
      ReceivedSharedImage{decode_id, mailbox, size, visible_rect});

  if (TrackedCallback::IsPending(get_picture_callback_)) {
    // The plugin may call GetPicture in its callback.
    scoped_refptr<TrackedCallback> callback;
    callback.swap(get_picture_callback_);
    WriteNextSharedImage();
    callback->Run(PP_OK);
  }
}

void VideoDecoderResource::OnPluginMsgNotifyError(
    const ResourceMessageReplyParams& params,
    int32_t error) {
  decoder_last_error_ = error;
  // Cause any pending callbacks to run immediately. Reentrancy isn't a problem,
  // since the plugin wasn't calling us.
  RunCallbackWithError(&initialize_callback_);
  RunCallbackWithError(&decode_callback_);
  RunCallbackWithError(&get_picture_callback_);
  RunCallbackWithError(&flush_callback_);
  RunCallbackWithError(&reset_callback_);
}

void VideoDecoderResource::OnPluginMsgInitializeComplete(
    const ResourceMessageReplyParams& params) {
  decoder_last_error_ = params.result();
  if (decoder_last_error_ == PP_OK) {
    initialized_ = true;
  }

  // Let the plugin call Initialize again from its callback in case of failure.
  scoped_refptr<TrackedCallback> callback;
  callback.swap(initialize_callback_);
  callback->Run(decoder_last_error_);
}

void VideoDecoderResource::OnPluginMsgDecodeComplete(
    const ResourceMessageReplyParams& params,
    uint32_t shm_id) {
  if (shm_id >= shm_buffers_.size()) {
    NOTREACHED();
  }
  // Make the shm buffer available.
  available_shm_buffers_.push_back(shm_buffers_[shm_id].get());
  // If the plugin is waiting, let it call Decode again.
  if (decode_callback_.get()) {
    scoped_refptr<TrackedCallback> callback;
    callback.swap(decode_callback_);
    callback->Run(PP_OK);
  }
}

void VideoDecoderResource::OnPluginMsgFlushComplete(
    const ResourceMessageReplyParams& params) {
  // All shm buffers should have been made available by now.
  DCHECK_EQ(shm_buffers_.size(), available_shm_buffers_.size());

  if (get_picture_callback_.get()) {
    scoped_refptr<TrackedCallback> callback;
    callback.swap(get_picture_callback_);
    callback->Abort();
  }

  scoped_refptr<TrackedCallback> callback;
  callback.swap(flush_callback_);
  callback->Run(params.result());
}

void VideoDecoderResource::OnPluginMsgResetComplete(
    const ResourceMessageReplyParams& params) {
  // All shm buffers should have been made available by now.
  DCHECK_EQ(shm_buffers_.size(), available_shm_buffers_.size());

  // Recycle any pictures which haven't been passed to the plugin.
  while (!received_shared_images_.empty()) {
    Post(RENDERER, PpapiHostMsg_VideoDecoder_RecycleSharedImage(
                       received_shared_images_.front().mailbox));
    received_shared_images_.pop();
  }

  scoped_refptr<TrackedCallback> callback;
  callback.swap(reset_callback_);
  callback->Run(params.result());
}

void VideoDecoderResource::RunCallbackWithError(
    scoped_refptr<TrackedCallback>* callback) {
  SafeRunCallback(callback, decoder_last_error_);
}

void VideoDecoderResource::WriteNextSharedImage() {
  CHECK(!received_shared_images_.empty());
  auto& shared_image = received_shared_images_.front();

  // Internally, we identify decodes by a unique id, which the host returns
  // to us in the picture. Use this to get the plugin's decode_id.
  uint32_t decode_id =
      decode_ids_[shared_image.decode_id % kMaximumPictureDelay];
  uint32_t texture_id;

  if (testing_) {
    // In unit tests we don't have gles2_impl_, so just generate ids
    // sequentially.
    static uint32_t texture_ids_for_testing = 1;
    texture_id = texture_ids_for_testing++;
  } else {
    CHECK(gles2_impl_);
    // Plugin's GLES2Interface and Renderer's RasterInterface are synchronized
    // by issued `ShallowFlushCHROMIUM` after each work. We get shared image
    // here after VideoDecoderShim copies new content to it on RasterInterface
    // and the context provider is flushed, so we don't need to wait on
    // SyncToken here.
    texture_id = base::strict_cast<uint32_t>(
        gles2_impl_->CreateAndTexStorage2DSharedImageCHROMIUM(
            shared_image.mailbox.name));
    gles2_impl_->BeginSharedImageAccessDirectCHROMIUM(
        texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

    // Flush our GLES2Interface to synchronize with the one that the plugin has.
    // They are in a same share group.
    gles2_impl_->Flush();
  }

  if (get_picture_) {
    DCHECK(!get_picture_0_1_);
    get_picture_->decode_id = decode_id;
    get_picture_->texture_id = texture_id;
    get_picture_->texture_target = GL_TEXTURE_2D;
    get_picture_->texture_size = shared_image.size;
    get_picture_->visible_rect = shared_image.visible_rect;
    get_picture_ = nullptr;
  } else {
    DCHECK(get_picture_0_1_);
    get_picture_0_1_->decode_id = decode_id;
    get_picture_0_1_->texture_id = texture_id;
    get_picture_0_1_->texture_target = GL_TEXTURE_2D;
    get_picture_0_1_->texture_size = shared_image.size;
    get_picture_0_1_ = nullptr;
  }

  used_shared_images_.insert(std::make_pair(texture_id, shared_image));

  received_shared_images_.pop();
}

}  // namespace proxy
}  // namespace ppapi
