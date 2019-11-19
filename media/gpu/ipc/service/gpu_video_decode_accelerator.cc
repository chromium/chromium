// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/message_filter.h"
#include "media/base/limits.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/ipc/common/media_messages.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"

namespace media {

namespace {
static gl::GLContext* GetGLContext(
    const base::WeakPtr<gpu::CommandBufferStub>& stub) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; no GLContext.";
    return nullptr;
  }

  return stub->decoder_context()->GetGLContext();
}

static bool MakeDecoderContextCurrent(
    const base::WeakPtr<gpu::CommandBufferStub>& stub) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; won't MakeCurrent().";
    return false;
  }

  if (!stub->decoder_context()->MakeCurrent()) {
    DLOG(ERROR) << "Failed to MakeCurrent()";
    return false;
  }

  return true;
}

static bool BindImage(const base::WeakPtr<gpu::CommandBufferStub>& stub,
                      uint32_t client_texture_id,
                      uint32_t texture_target,
                      const scoped_refptr<gl::GLImage>& image,
                      bool can_bind_to_sampler) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; won't BindImage().";
    return false;
  }

  gpu::DecoderContext* command_decoder = stub->decoder_context();
  command_decoder->BindImage(client_texture_id, texture_target, image.get(),
                             can_bind_to_sampler);
  return true;
}

static gpu::gles2::ContextGroup* GetContextGroup(
    const base::WeakPtr<gpu::CommandBufferStub>& stub) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; no DecoderContext.";
    return nullptr;
  }

  return stub->decoder_context()->GetContextGroup();
}

static std::unique_ptr<gpu::gles2::AbstractTexture> CreateAbstractTexture(
    const base::WeakPtr<gpu::CommandBufferStub>& stub,
    GLenum target,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; no DecoderContext.";
    return nullptr;
  }

  return stub->decoder_context()->CreateAbstractTexture(
      target, internal_format, width, height, depth, border, format, type);
}

}  // anonymous namespace

// DebugAutoLock works like AutoLock but only acquires the lock when
// DCHECK is on.
#if DCHECK_IS_ON()
typedef base::AutoLock DebugAutoLock;
#else
class DebugAutoLock {
 public:
  explicit DebugAutoLock(base::Lock&) {}
};
#endif

class GpuVideoDecodeAccelerator::MessageFilter : public IPC::MessageFilter {
 public:
  MessageFilter(GpuVideoDecodeAccelerator* owner, int32_t host_route_id)
      : owner_(owner), host_route_id_(host_route_id) {}

  void OnChannelError() override { sender_ = NULL; }

  void OnChannelClosing() override { sender_ = NULL; }

  void OnFilterAdded(IPC::Channel* channel) override { sender_ = channel; }

  void OnFilterRemoved() override {
    // This will delete |owner_| and |this|.
    owner_->OnFilterRemoved();
  }

  bool OnMessageReceived(const IPC::Message& msg) override {
    if (msg.routing_id() != host_route_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP(MessageFilter, msg)
      IPC_MESSAGE_FORWARD(AcceleratedVideoDecoderMsg_Decode, owner_,
                          GpuVideoDecodeAccelerator::OnDecode)
      IPC_MESSAGE_UNHANDLED(return false)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  bool SendOnIOThread(IPC::Message* message) {
    DCHECK(!message->is_sync());
    if (!sender_) {
      delete message;
      return false;
    }
    return sender_->Send(message);
  }

 protected:
  ~MessageFilter() override = default;

 private:
  GpuVideoDecodeAccelerator* const owner_;
  const int32_t host_route_id_;
  // The sender to which this filter was added.
  IPC::Sender* sender_;
};

GpuVideoDecodeAccelerator::GpuVideoDecodeAccelerator(
    int32_t host_route_id,
    gpu::CommandBufferStub* stub,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const AndroidOverlayMojoFactoryCB& overlay_factory_cb)
    : host_route_id_(host_route_id),
      stub_(stub),
      texture_target_(0),
      pixel_format_(PIXEL_FORMAT_UNKNOWN),
      textures_per_buffer_(0),
      filter_removed_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      overlay_factory_cb_(overlay_factory_cb) {
  DCHECK(stub_);
  stub_->AddDestructionObserver(this);
  get_gl_context_cb_ = base::BindRepeating(&GetGLContext, stub_->AsWeakPtr());
  make_context_current_cb_ =
      base::BindRepeating(&MakeDecoderContextCurrent, stub_->AsWeakPtr());
  bind_image_cb_ = base::BindRepeating(&BindImage, stub_->AsWeakPtr());
  get_context_group_cb_ =
      base::BindRepeating(&GetContextGroup, stub_->AsWeakPtr());
  create_abstract_texture_cb_ =
      base::BindRepeating(&CreateAbstractTexture, stub_->AsWeakPtr());
}

GpuVideoDecodeAccelerator::~GpuVideoDecodeAccelerator() {
  // This class can only be self-deleted from OnWillDestroyStub(), which means
  // the VDA has already been destroyed in there.
  DCHECK(!video_decode_accelerator_);
}

// static
gpu::VideoDecodeAcceleratorCapabilities
GpuVideoDecodeAccelerator::GetCapabilities(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  return GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities(
      gpu_preferences, workarounds);
}

bool GpuVideoDecodeAccelerator::OnMessageReceived(const IPC::Message& msg) {
  if (!video_decode_accelerator_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAccelerator, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Decode, OnDecode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_AssignPictureBuffers,
                        OnAssignPictureBuffers)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                        OnReusePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Flush, OnFlush)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_SetOverlayInfo,
                        OnSetOverlayInfo)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuVideoDecodeAccelerator::NotifyInitializationComplete(bool success) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_InitializationComplete(
          host_route_id_, success)))
    DLOG(ERROR)
        << "Send(AcceleratedVideoDecoderHostMsg_InitializationComplete) failed";
}

void GpuVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  if (dimensions.width() > limits::kMaxDimension ||
      dimensions.height() > limits::kMaxDimension ||
      dimensions.GetArea() > limits::kMaxCanvas) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  if (!Send(new AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers(
          host_route_id_, requested_num_of_buffers, format, textures_per_buffer,
          dimensions, texture_target))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers) "
                << "failed";
  }
  texture_dimensions_ = dimensions;
  textures_per_buffer_ = textures_per_buffer;
  texture_target_ = texture_target;
  pixel_format_ = format;
}

void GpuVideoDecodeAccelerator::DismissPictureBuffer(
    int32_t picture_buffer_id) {
  // Notify client that picture buffer is now unused.
  if (!Send(new AcceleratedVideoDecoderHostMsg_DismissPictureBuffer(
          host_route_id_, picture_buffer_id))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer) "
                << "failed";
  }
  DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
  uncleared_textures_.erase(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::PictureReady(const Picture& picture) {
  // VDA may call PictureReady on IO thread. SetTextureCleared should run on
  // the child thread. VDA is responsible to call PictureReady on the child
  // thread when a picture buffer is delivered the first time.
  if (child_task_runner_->BelongsToCurrentThread()) {
    SetTextureCleared(picture);
  } else {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
    DCHECK_EQ(0u, uncleared_textures_.count(picture.picture_buffer_id()));
  }

  AcceleratedVideoDecoderHostMsg_PictureReady_Params params;
  params.picture_buffer_id = picture.picture_buffer_id();
  params.bitstream_buffer_id = picture.bitstream_buffer_id();
  params.visible_rect = picture.visible_rect();
  params.color_space = picture.color_space();
  params.allow_overlay = picture.allow_overlay();
  params.read_lock_fences_enabled = picture.read_lock_fences_enabled();
  params.size_changed = picture.size_changed();
  params.surface_texture = picture.texture_owner();
  params.wants_promotion_hint = picture.wants_promotion_hint();
  if (!Send(new AcceleratedVideoDecoderHostMsg_PictureReady(host_route_id_,
                                                            params))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_PictureReady) failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed(
          host_route_id_, bitstream_buffer_id))) {
    DLOG(ERROR)
        << "Send(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed) "
        << "failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyFlushDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_FlushDone(host_route_id_)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_FlushDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyResetDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ResetDone(host_route_id_)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ResetDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyError(
    VideoDecodeAccelerator::Error error) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ErrorNotification(host_route_id_,
                                                                 error))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ErrorNotification) "
                << "failed";
  }
}

void GpuVideoDecodeAccelerator::OnWillDestroyStub(bool have_context) {
  // The stub is going away, so we have to stop and destroy VDA here, before
  // returning, because the VDA may need the GL context to run and/or do its
  // cleanup. We cannot destroy the VDA before the IO thread message filter is
  // removed however, since we cannot service incoming messages with VDA gone.
  // We cannot simply check for existence of VDA on IO thread though, because
  // we don't want to synchronize the IO thread with the ChildThread.
  // So we have to wait for the RemoveFilter callback here instead and remove
  // the VDA after it arrives and before returning.
  if (filter_) {
    stub_->channel()->RemoveFilter(filter_.get());
    filter_removed_.Wait();
  }

  stub_->channel()->RemoveRoute(host_route_id_);
  stub_->RemoveDestructionObserver(this);

  video_decode_accelerator_.reset();
  delete this;
}

bool GpuVideoDecodeAccelerator::Send(IPC::Message* message) {
  if (filter_ && io_task_runner_->BelongsToCurrentThread())
    return filter_->SendOnIOThread(message);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  return stub_->channel()->Send(message);
}

bool GpuVideoDecodeAccelerator::Initialize(
    const VideoDecodeAccelerator::Config& config) {
  DCHECK(!video_decode_accelerator_);

  if (!stub_->channel()->AddRoute(host_route_id_, stub_->sequence_id(), this)) {
    DLOG(ERROR) << "Initialize(): failed to add route";
    return false;
  }

#if !defined(OS_WIN)
  // Ensure we will be able to get a GL context at all before initializing
  // non-Windows VDAs.
  if (!make_context_current_cb_.Run())
    return false;
#endif

  std::unique_ptr<GpuVideoDecodeAcceleratorFactory> vda_factory =
      GpuVideoDecodeAcceleratorFactory::CreateWithGLES2Decoder(
          get_gl_context_cb_, make_context_current_cb_, bind_image_cb_,
          get_context_group_cb_, overlay_factory_cb_,
          create_abstract_texture_cb_);

  if (!vda_factory) {
    LOG(ERROR) << "Failed creating the VDA factory";
    return false;
  }

  const gpu::GpuDriverBugWorkarounds& gpu_workarounds =
      stub_->channel()->gpu_channel_manager()->gpu_driver_bug_workarounds();
  const gpu::GpuPreferences& gpu_preferences =
      stub_->channel()->gpu_channel_manager()->gpu_preferences();
  video_decode_accelerator_ =
      vda_factory->CreateVDA(this, config, gpu_workarounds, gpu_preferences);
  if (!video_decode_accelerator_) {
    LOG(ERROR) << "HW video decode not available for profile "
               << GetProfileName(config.profile)
               << (config.is_encrypted() ? " with encryption" : "");
    return false;
  }

  // Attempt to set up performing decoding tasks on IO thread, if supported by
  // the VDA.
  if (video_decode_accelerator_->TryToSetupDecodeOnSeparateThread(
          weak_factory_for_io_.GetWeakPtr(), io_task_runner_)) {
    filter_ = new MessageFilter(this, host_route_id_);
    stub_->channel()->AddFilter(filter_.get());
  }

  return true;
}

// Runs on IO thread if VDA::TryToSetupDecodeOnSeparateThread() succeeded,
// otherwise on the main thread.
void GpuVideoDecodeAccelerator::OnDecode(BitstreamBuffer bitstream_buffer) {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->Decode(std::move(bitstream_buffer));
}

void GpuVideoDecodeAccelerator::OnAssignPictureBuffers(
    const std::vector<int32_t>& buffer_ids,
    const std::vector<PictureBuffer::TextureIds>& texture_ids) {
  if (buffer_ids.size() != texture_ids.size()) {
    NotifyError(VideoDecodeAccelerator::INVALID_ARGUMENT);
    return;
  }

  gpu::DecoderContext* decoder_context = stub_->decoder_context();
  gpu::gles2::TextureManager* texture_manager =
      stub_->decoder_context()->GetContextGroup()->texture_manager();

  std::vector<PictureBuffer> buffers;
  std::vector<std::vector<scoped_refptr<gpu::gles2::TextureRef>>> textures;
  for (uint32_t i = 0; i < buffer_ids.size(); ++i) {
    if (buffer_ids[i] < 0) {
      DLOG(ERROR) << "Buffer id " << buffer_ids[i] << " out of range";
      NotifyError(VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    std::vector<scoped_refptr<gpu::gles2::TextureRef>> current_textures;
    PictureBuffer::TextureIds buffer_texture_ids = texture_ids[i];
    PictureBuffer::TextureIds service_ids;
    if (buffer_texture_ids.size() != textures_per_buffer_) {
      DLOG(ERROR) << "Requested " << textures_per_buffer_
                  << " textures per picture buffer, got "
                  << buffer_texture_ids.size();
      NotifyError(VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    for (size_t j = 0; j < textures_per_buffer_; j++) {
      gpu::TextureBase* texture_base =
          decoder_context->GetTextureBase(buffer_texture_ids[j]);
      if (!texture_base) {
        DLOG(ERROR) << "Failed to find texture id " << buffer_texture_ids[j];
        NotifyError(VideoDecodeAccelerator::INVALID_ARGUMENT);
        return;
      }

      if (texture_base->target() != texture_target_) {
        DLOG(ERROR) << "Texture target mismatch for texture id "
                    << buffer_texture_ids[j];
        NotifyError(VideoDecodeAccelerator::INVALID_ARGUMENT);
        return;
      }

      gpu::gles2::TextureRef* texture_ref =
          texture_manager->GetTexture(buffer_texture_ids[j]);
      if (texture_ref) {
        gpu::gles2::Texture* info = texture_ref->texture();
        if (texture_target_ == GL_TEXTURE_EXTERNAL_OES ||
            texture_target_ == GL_TEXTURE_RECTANGLE_ARB) {
          // These textures have their dimensions defined by the underlying
          // storage.
          // Use |texture_dimensions_| for this size.
          texture_manager->SetLevelInfo(texture_ref, texture_target_, 0,
                                        GL_RGBA, texture_dimensions_.width(),
                                        texture_dimensions_.height(), 1, 0,
                                        GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
        } else {
          // For other targets, texture dimensions should already be defined.
          GLsizei width = 0, height = 0;
          info->GetLevelSize(texture_target_, 0, &width, &height, nullptr);
          if (width != texture_dimensions_.width() ||
              height != texture_dimensions_.height()) {
            DLOG(ERROR) << "Size mismatch for texture id "
                        << buffer_texture_ids[j];
            NotifyError(VideoDecodeAccelerator::INVALID_ARGUMENT);
            return;
          }

          // TODO(dshwang): after moving to D3D11, remove this.
          // https://crbug.com/438691
          GLenum format = video_decode_accelerator_->GetSurfaceInternalFormat();
          if (format != GL_RGBA) {
            DCHECK(format == GL_BGRA_EXT);
            texture_manager->SetLevelInfo(texture_ref, texture_target_, 0,
                                          format, width, height, 1, 0, format,
                                          GL_UNSIGNED_BYTE, gfx::Rect());
          }
        }
        current_textures.push_back(texture_ref);
      }
      service_ids.push_back(texture_base->service_id());
    }
    textures.push_back(current_textures);
    buffers.push_back(PictureBuffer(buffer_ids[i], texture_dimensions_,
                                    buffer_texture_ids, service_ids,
                                    texture_target_, pixel_format_));
  }
  {
    DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
    for (uint32_t i = 0; i < buffer_ids.size(); ++i)
      uncleared_textures_[buffer_ids[i]] = textures[i];
  }
  video_decode_accelerator_->AssignPictureBuffers(buffers);
}

void GpuVideoDecodeAccelerator::OnReusePictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->ReusePictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::OnFlush() {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->Flush();
}

void GpuVideoDecodeAccelerator::OnReset() {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->Reset();
}

void GpuVideoDecodeAccelerator::OnSetOverlayInfo(
    const OverlayInfo& overlay_info) {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->SetOverlayInfo(overlay_info);
}

void GpuVideoDecodeAccelerator::OnDestroy() {
  DCHECK(video_decode_accelerator_);
  OnWillDestroyStub(false);
}

void GpuVideoDecodeAccelerator::OnFilterRemoved() {
  // We're destroying; cancel all callbacks.
  weak_factory_for_io_.InvalidateWeakPtrs();
  filter_removed_.Signal();
}

void GpuVideoDecodeAccelerator::SetTextureCleared(const Picture& picture) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
  auto it = uncleared_textures_.find(picture.picture_buffer_id());
  if (it == uncleared_textures_.end())
    return;  // the texture has been cleared

  for (auto texture_ref : it->second) {
    GLenum target = texture_ref->texture()->target();
    gpu::gles2::TextureManager* texture_manager =
        stub_->decoder_context()->GetContextGroup()->texture_manager();
    texture_manager->SetLevelCleared(texture_ref.get(), target, 0, true);
  }
  uncleared_textures_.erase(it);
}

}  // namespace media
