// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
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
#include "mojo/public/cpp/bindings/associated_receiver.h"
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
static bool BindDecoderManagedImage(
    const base::WeakPtr<gpu::CommandBufferStub>& stub,
    uint32_t client_texture_id,
    uint32_t texture_target,
    const scoped_refptr<gl::GLImage>& image) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; won't BindImage().";
    return false;
  }

  gpu::DecoderContext* command_decoder = stub->decoder_context();
  command_decoder->AttachImageToTextureWithDecoderBinding(
      client_texture_id, texture_target, image.get());
  return true;
}
#else
static bool BindClientManagedImage(
    const base::WeakPtr<gpu::CommandBufferStub>& stub,
    uint32_t client_texture_id,
    uint32_t texture_target,
    const scoped_refptr<gl::GLImage>& image) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; won't BindImage().";
    return false;
  }

  gpu::DecoderContext* command_decoder = stub->decoder_context();
  command_decoder->AttachImageToTextureWithClientBinding(
      client_texture_id, texture_target, image.get());

  return true;
}
#endif

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

// Receives incoming messages for the decoder. Operates exclusively on the IO
// thread, since sometimes we want to do decodes directly from there.
class GpuVideoDecodeAccelerator::MessageFilter
    : public mojom::GpuAcceleratedVideoDecoder {
 public:
  MessageFilter(GpuVideoDecodeAccelerator* owner,
                scoped_refptr<base::SequencedTaskRunner> owner_task_runner,
                bool decode_on_io)
      : owner_(owner),
        owner_task_runner_(std::move(owner_task_runner)),
        decode_on_io_(decode_on_io) {}
  ~MessageFilter() override = default;

  // Called from the main thread. Posts to `io_task_runner` to do the binding
  // and waits for completion before returning. This ensures the decoder's
  // endpoint is established before the synchronous request to establish it is
  // acknowledged to the client.
  bool Bind(mojo::PendingAssociatedReceiver<mojom::GpuAcceleratedVideoDecoder>
                receiver,
            const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
    base::WaitableEvent bound_event;
    if (!io_task_runner->PostTask(
            FROM_HERE, base::BindOnce(&MessageFilter::BindOnIoThread,
                                      base::Unretained(this),
                                      std::move(receiver), &bound_event))) {
      return false;
    }
    bound_event.Wait();
    return true;
  }

  // Must be called on the IO thread. Posts back to the owner's task runner to
  // destroy it.
  void RequestShutdown() {
    if (!owner_)
      return;

    // Must be reset here on the IO thread before `this` is destroyed.
    receiver_.reset();

    GpuVideoDecodeAccelerator* owner = owner_;
    owner_ = nullptr;

    // Invalidate any IO thread WeakPtrs which may be held by the
    // VideoDecodeAccelerator, and post to delete our owner which will in turn
    // delete us. Note that it is unsafe to access any members of `this` once
    // the task below is posted.
    owner->weak_factory_for_io_.InvalidateWeakPtrs();
    owner_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuVideoDecodeAccelerator::DeleteSelfNow,
                                  base::Unretained(owner)));
  }

  // mojom::GpuAcceleratedVideoDecoder:
  void Decode(BitstreamBuffer buffer) override;
  void AssignPictureBuffers(
      std::vector<mojom::PictureBufferAssignmentPtr> assignments) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush(FlushCallback callback) override;
  void Reset(ResetCallback callback) override;
  void SetOverlayInfo(const OverlayInfo& overlay_info) override;

 private:
  void BindOnIoThread(mojo::PendingAssociatedReceiver<
                          mojom::GpuAcceleratedVideoDecoder> receiver,
                      base::WaitableEvent* bound_event) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&MessageFilter::OnDisconnect, base::Unretained(this)));
    bound_event->Signal();
  }

  void OnDisconnect() {
    if (!owner_)
      return;

    owner_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuVideoDecodeAccelerator::OnDestroy,
                                  base::Unretained(owner_)));
  }

  raw_ptr<GpuVideoDecodeAccelerator> owner_;
  const scoped_refptr<base::SequencedTaskRunner> owner_task_runner_;
  const bool decode_on_io_;
  mojo::AssociatedReceiver<mojom::GpuAcceleratedVideoDecoder> receiver_{this};
};

void GpuVideoDecodeAccelerator::MessageFilter::Decode(BitstreamBuffer buffer) {
  if (!owner_)
    return;

  if (decode_on_io_) {
    owner_->OnDecode(std::move(buffer));
  } else {
    owner_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuVideoDecodeAccelerator::OnDecode,
                                  base::Unretained(owner_), std::move(buffer)));
  }
}

void GpuVideoDecodeAccelerator::MessageFilter::AssignPictureBuffers(
    std::vector<mojom::PictureBufferAssignmentPtr> assignments) {
  if (!owner_)
    return;
  owner_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoDecodeAccelerator::OnAssignPictureBuffers,
                     base::Unretained(owner_), std::move(assignments)));
}

void GpuVideoDecodeAccelerator::MessageFilter::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  if (!owner_)
    return;
  owner_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoDecodeAccelerator::OnReusePictureBuffer,
                     base::Unretained(owner_), picture_buffer_id));
}

void GpuVideoDecodeAccelerator::MessageFilter::Flush(FlushCallback callback) {
  if (!owner_)
    return;
  owner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuVideoDecodeAccelerator::OnFlush,
                                base::Unretained(owner_), std::move(callback)));
}

void GpuVideoDecodeAccelerator::MessageFilter::Reset(ResetCallback callback) {
  if (!owner_)
    return;
  owner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuVideoDecodeAccelerator::OnReset,
                                base::Unretained(owner_), std::move(callback)));
}

void GpuVideoDecodeAccelerator::MessageFilter::SetOverlayInfo(
    const OverlayInfo& overlay_info) {
  if (!owner_)
    return;
  owner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuVideoDecodeAccelerator::OnSetOverlayInfo,
                                base::Unretained(owner_), overlay_info));
}

GpuVideoDecodeAccelerator::GpuVideoDecodeAccelerator(
    gpu::CommandBufferStub* stub,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const AndroidOverlayMojoFactoryCB& overlay_factory_cb)
    : stub_(stub),
      texture_target_(0),
      pixel_format_(PIXEL_FORMAT_UNKNOWN),
      textures_per_buffer_(0),
      child_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      io_task_runner_(io_task_runner),
      overlay_factory_cb_(overlay_factory_cb) {
  DCHECK(stub_);
  stub_->AddDestructionObserver(this);
  gl_client_.get_context =
      base::BindRepeating(&GetGLContext, stub_->AsWeakPtr());
  gl_client_.make_context_current =
      base::BindRepeating(&MakeDecoderContextCurrent, stub_->AsWeakPtr());
  // The semantics of |bind_image| vary per-platform: On Windows and Mac it must
  // mark the image as needing binding by the decoder, while on other platforms
  // it must mark the image as *not* needing binding by the decoder.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  gl_client_.bind_image =
      base::BindRepeating(&BindDecoderManagedImage, stub_->AsWeakPtr());
#else
  gl_client_.bind_image =
      base::BindRepeating(&BindClientManagedImage, stub_->AsWeakPtr());
#endif
  gl_client_.get_context_group =
      base::BindRepeating(&GetContextGroup, stub_->AsWeakPtr());
  gl_client_.create_abstract_texture =
      base::BindRepeating(&CreateAbstractTexture, stub_->AsWeakPtr());
  gl_client_.is_passthrough =
      stub_->decoder_context()->GetFeatureInfo()->is_passthrough_cmd_decoder();
  gl_client_.supports_arb_texture_rectangle = stub_->decoder_context()
                                                  ->GetFeatureInfo()
                                                  ->feature_flags()
                                                  .arb_texture_rectangle;
}

GpuVideoDecodeAccelerator::~GpuVideoDecodeAccelerator() {
  // This class can only be self-deleted from OnWillDestroyStub(), which means
  // the VDA has already been destroyed in there.
  DCHECK(!video_decode_accelerator_);
}

void GpuVideoDecodeAccelerator::DeleteSelfNow() {
  delete this;
}

// static
gpu::VideoDecodeAcceleratorCapabilities
GpuVideoDecodeAccelerator::GetCapabilities(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  return GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities(
      gpu_preferences, workarounds);
}

void GpuVideoDecodeAccelerator::NotifyInitializationComplete(
    DecoderStatus status) {
  decoder_client_->OnInitializationComplete(status.is_ok());
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

  texture_dimensions_ = dimensions;
  textures_per_buffer_ = textures_per_buffer;
  texture_target_ = texture_target;
  pixel_format_ = format;

  decoder_client_->OnProvidePictureBuffers(requested_num_of_buffers, format,
                                           textures_per_buffer, dimensions,
                                           texture_target);
}

void GpuVideoDecodeAccelerator::DismissPictureBuffer(
    int32_t picture_buffer_id) {
  // Notify client that picture buffer is now unused.
  decoder_client_->OnDismissPictureBuffer(picture_buffer_id);
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

  auto params = mojom::PictureReadyParams::New();
  params->picture_buffer_id = picture.picture_buffer_id();
  params->bitstream_buffer_id = picture.bitstream_buffer_id();
  params->visible_rect = picture.visible_rect();
  params->color_space = picture.color_space();
  params->allow_overlay = picture.allow_overlay();
  params->read_lock_fences_enabled = picture.read_lock_fences_enabled();
  params->size_changed = picture.size_changed();
  params->surface_texture = picture.texture_owner();
  params->wants_promotion_hint = picture.wants_promotion_hint();
  decoder_client_->OnPictureReady(std::move(params));
}

void GpuVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  decoder_client_->OnBitstreamBufferProcessed(bitstream_buffer_id);
}

void GpuVideoDecodeAccelerator::NotifyFlushDone() {
  DCHECK(!pending_flushes_.empty());
  std::move(pending_flushes_.front()).Run();
  pending_flushes_.pop_front();
}

void GpuVideoDecodeAccelerator::NotifyResetDone() {
  DCHECK(!pending_resets_.empty());
  std::move(pending_resets_.front()).Run();
  pending_resets_.pop_front();
}

void GpuVideoDecodeAccelerator::NotifyError(
    VideoDecodeAccelerator::Error error) {
  decoder_client_->OnError(error);
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
  stub_->RemoveDestructionObserver(this);
  if (filter_) {
    io_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&MessageFilter::RequestShutdown,
                                             base::Unretained(filter_.get())));
  }

  video_decode_accelerator_.reset();
}

bool GpuVideoDecodeAccelerator::Initialize(
    const VideoDecodeAccelerator::Config& config,
    mojo::PendingAssociatedReceiver<mojom::GpuAcceleratedVideoDecoder> receiver,
    mojo::PendingAssociatedRemote<mojom::GpuAcceleratedVideoDecoderClient>
        client) {
  DCHECK(!video_decode_accelerator_);

#if !BUILDFLAG(IS_WIN)
  // Ensure we will be able to get a GL context at all before initializing
  // non-Windows VDAs.
  if (!gl_client_.make_context_current.Run())
    return false;
#endif

  std::unique_ptr<GpuVideoDecodeAcceleratorFactory> vda_factory =
      GpuVideoDecodeAcceleratorFactory::Create(gl_client_);
  if (!vda_factory) {
    LOG(ERROR) << "Failed creating the VDA factory";
    return false;
  }

  const gpu::GpuDriverBugWorkarounds& gpu_workarounds =
      stub_->channel()->gpu_channel_manager()->gpu_driver_bug_workarounds();
  const gpu::GpuPreferences& gpu_preferences =
      stub_->channel()->gpu_channel_manager()->gpu_preferences();

  if (config.output_mode !=
      VideoDecodeAccelerator::Config::OutputMode::ALLOCATE) {
    DLOG(ERROR) << "Only ALLOCATE mode is supported";
    return false;
  }

  video_decode_accelerator_ =
      vda_factory->CreateVDA(this, config, gpu_workarounds, gpu_preferences);
  if (!video_decode_accelerator_) {
    LOG(ERROR) << "HW video decode not available for profile "
               << GetProfileName(config.profile)
               << (config.is_encrypted() ? " with encryption" : "");
    return false;
  }

  decoder_client_.Bind(std::move(client), io_task_runner_);

  // Attempt to set up performing decoding tasks on IO thread, if supported by
  // the VDA.
  bool decode_on_io =
      video_decode_accelerator_->TryToSetupDecodeOnSeparateThread(
          weak_factory_for_io_.GetWeakPtr(), io_task_runner_);

  // Bind the receiver on the IO thread. We wait here for it to be bound
  // before returning and signaling that the decoder has been created.
  filter_ =
      std::make_unique<MessageFilter>(this, stub_->task_runner(), decode_on_io);
  return filter_->Bind(std::move(receiver), io_task_runner_);
}

// Runs on IO thread if VDA::TryToSetupDecodeOnSeparateThread() succeeded,
// otherwise on the main thread.
void GpuVideoDecodeAccelerator::OnDecode(BitstreamBuffer bitstream_buffer) {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->Decode(std::move(bitstream_buffer));
}

void GpuVideoDecodeAccelerator::OnAssignPictureBuffers(
    std::vector<mojom::PictureBufferAssignmentPtr> assignments) {
  gpu::DecoderContext* decoder_context = stub_->decoder_context();
  gpu::gles2::TextureManager* texture_manager =
      stub_->decoder_context()->GetContextGroup()->texture_manager();

  std::vector<PictureBuffer> buffers;
  std::vector<std::vector<scoped_refptr<gpu::gles2::TextureRef>>> textures;
  for (const auto& assignment : assignments) {
    if (assignment->buffer_id < 0) {
      DLOG(ERROR) << "Buffer id " << assignment->buffer_id << " out of range";
      NotifyError(VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    std::vector<scoped_refptr<gpu::gles2::TextureRef>> current_textures;
    PictureBuffer::TextureIds buffer_texture_ids = assignment->texture_ids;
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

      if (texture_manager) {
        gpu::gles2::TextureRef* texture_ref =
            texture_manager->GetTexture(buffer_texture_ids[j]);
        if (texture_ref) {
          gpu::gles2::Texture* info = texture_ref->texture();
          if (texture_target_ == GL_TEXTURE_EXTERNAL_OES ||
              texture_target_ == GL_TEXTURE_RECTANGLE_ARB) {
            // These textures have their dimensions defined by the underlying
            // storage.
            // Use |texture_dimensions_| for this size.
            texture_manager->SetLevelInfo(
                texture_ref, texture_target_, 0, GL_RGBA,
                texture_dimensions_.width(), texture_dimensions_.height(), 1, 0,
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
            GLenum format =
                video_decode_accelerator_->GetSurfaceInternalFormat();
            if (format != GL_RGBA) {
              DCHECK(format == GL_BGRA_EXT);
              texture_manager->SetLevelInfo(texture_ref, texture_target_, 0,
                                            format, width, height, 1, 0, format,
                                            GL_UNSIGNED_BYTE, gfx::Rect());
            }
          }
          current_textures.push_back(texture_ref);
        }
      }
      service_ids.push_back(texture_base->service_id());
    }
    textures.push_back(current_textures);
    buffers.emplace_back(assignment->buffer_id, texture_dimensions_,
                         buffer_texture_ids, service_ids, texture_target_,
                         pixel_format_);
  }
  {
    DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
    for (uint32_t i = 0; i < assignments.size(); ++i)
      uncleared_textures_[assignments[i]->buffer_id] = textures[i];
  }
  video_decode_accelerator_->AssignPictureBuffers(buffers);
}

void GpuVideoDecodeAccelerator::OnReusePictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->ReusePictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::OnFlush(base::OnceClosure callback) {
  DCHECK(video_decode_accelerator_);
  pending_flushes_.push_back(
      base::BindPostTask(io_task_runner_, std::move(callback)));
  video_decode_accelerator_->Flush();
}

void GpuVideoDecodeAccelerator::OnReset(base::OnceClosure callback) {
  DCHECK(video_decode_accelerator_);
  pending_resets_.push_back(
      base::BindPostTask(io_task_runner_, std::move(callback)));
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
