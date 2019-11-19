// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/direct_shared_image_video_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_video.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

scoped_refptr<gpu::SharedContextState> GetSharedContext(
    gpu::CommandBufferStub* stub,
    gpu::ContextResult* result) {
  auto shared_context =
      stub->channel()->gpu_channel_manager()->GetSharedContextState(result);
  return (*result == gpu::ContextResult::kSuccess) ? shared_context : nullptr;
}

void ContextStateResultUMA(gpu::ContextResult result) {
  base::UmaHistogramEnumeration(
      "Media.GpuSharedImageVideoFactory.SharedContextStateResult", result);
}

}  // namespace

using gpu::gles2::AbstractTexture;

DirectSharedImageVideoProvider::DirectSharedImageVideoProvider(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCB get_stub_cb)
    : gpu_factory_(gpu_task_runner, std::move(get_stub_cb)),
      gpu_task_runner_(std::move(gpu_task_runner)) {}

DirectSharedImageVideoProvider::~DirectSharedImageVideoProvider() = default;

// TODO(liberato): add a thread hop to create the default texture owner, but
// not as part of this class.  just post something from VideoFrameFactory.
void DirectSharedImageVideoProvider::Initialize(GpuInitCB gpu_init_cb) {
  // Note that we do not BindToCurrentLoop |gpu_init_cb|, since it is supposed
  // to be called on the gpu main thread, which is somewhat hacky.
  gpu_factory_.Post(FROM_HERE, &GpuSharedImageVideoFactory::Initialize,
                    std::move(gpu_init_cb));
}

void DirectSharedImageVideoProvider::RequestImage(
    ImageReadyCB cb,
    const ImageSpec& spec,
    scoped_refptr<gpu::TextureOwner> texture_owner) {
  // It's unclear that we should handle the image group, but since CodecImages
  // have to be registered on it, we do.  If the CodecImage is ever re-used,
  // then part of that re-use would be to call the (then mis-named)
  // destruction cb to remove it from the group.
  //
  // Also note that CodecImage shouldn't be the thing that's added to the
  // group anyway.  The thing that owns buffer management is all we really
  // care about, and that doesn't have anything to do with GLImage.

  gpu_factory_.Post(FROM_HERE, &GpuSharedImageVideoFactory::CreateImage,
                    BindToCurrentLoop(std::move(cb)), spec,
                    std::move(texture_owner));
}

GpuSharedImageVideoFactory::GpuSharedImageVideoFactory(
    SharedImageVideoProvider::GetStubCB get_stub_cb) {
  DETACH_FROM_THREAD(thread_checker_);
  stub_ = get_stub_cb.Run();
  if (stub_)
    stub_->AddDestructionObserver(this);
}

GpuSharedImageVideoFactory::~GpuSharedImageVideoFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_)
    stub_->RemoveDestructionObserver(this);
}

void GpuSharedImageVideoFactory::Initialize(
    SharedImageVideoProvider::GpuInitCB gpu_init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    std::move(gpu_init_cb).Run(nullptr);
    return;
  }

  decoder_helper_ = GLES2DecoderHelper::Create(stub_->decoder_context());

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    DLOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    ContextStateResultUMA(result);
    std::move(gpu_init_cb).Run(nullptr);
    return;
  }

  is_vulkan_ = shared_context->GrContextIsVulkan();

  // Make the shared context current.
  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      shared_context->context(), shared_context->surface());
  if (!shared_context->IsCurrent(nullptr)) {
    result = gpu::ContextResult::kTransientFailure;
    DLOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to make shared context current.";
    ContextStateResultUMA(result);
    std::move(gpu_init_cb).Run(nullptr);
    return;
  }

  // Note that if |gpu_init_cb| posts, then the ScopedMakeCurrent won't help.
  std::move(gpu_init_cb).Run(std::move(shared_context));
}

void GpuSharedImageVideoFactory::CreateImage(
    FactoryImageReadyCB image_ready_cb,
    const SharedImageVideoProvider::ImageSpec& spec,
    scoped_refptr<gpu::TextureOwner> texture_owner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Generate a shared image mailbox.
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();
  auto codec_image = base::MakeRefCounted<CodecImage>();

  TRACE_EVENT0("media", "GpuSharedImageVideoFactory::CreateVideoFrame");

  if (!CreateImageInternal(spec, std::move(texture_owner), mailbox,
                           codec_image)) {
    return;
  }

  // This callback destroys the shared image when video frame is
  // released/destroyed. This callback has a weak pointer to the shared image
  // stub because shared image stub could be destroyed before video frame. In
  // those cases there is no need to destroy the shared image as the shared
  // image stub destruction will cause all the shared images to be destroyed.
  auto destroy_shared_image =
      stub_->channel()->shared_image_stub()->GetSharedImageDestructionCallback(
          mailbox);

  // Guarantee that the SharedImage is destroyed even if the VideoFrame is
  // dropped. Otherwise we could keep shared images we don't need alive.
  auto release_cb = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      BindToCurrentLoop(std::move(destroy_shared_image)), gpu::SyncToken());

  SharedImageVideoProvider::ImageRecord record;
  record.mailbox = mailbox;
  record.release_cb = std::move(release_cb);
  record.is_vulkan = is_vulkan_;

  // Since |codec_image|'s ref holders can be destroyed by stub destruction, we
  // create a ref to it for the MaybeRenderEarlyManager.  This is a hack; we
  // should not be sending the CodecImage at all.  The MaybeRenderEarlyManager
  // should work with some other object that happens to be used by CodecImage,
  // and non-GL things, to hold the output buffer, etc.
  record.codec_image_holder = base::MakeRefCounted<CodecImageHolder>(
      base::SequencedTaskRunnerHandle::Get(), std::move(codec_image));

  std::move(image_ready_cb).Run(std::move(record));
}

bool GpuSharedImageVideoFactory::CreateImageInternal(
    const SharedImageVideoProvider::ImageSpec& spec,
    scoped_refptr<gpu::TextureOwner> texture_owner,
    gpu::Mailbox mailbox,
    scoped_refptr<CodecImage> image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_))
    return false;

  gpu::gles2::ContextGroup* group = stub_->decoder_context()->GetContextGroup();
  if (!group)
    return false;
  gpu::gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager)
    return false;

  const auto& size = spec.size;

  // Create a Texture and a CodecImage to back it.
  // TODO(liberato): Once legacy mailbox support is removed, we don't need to
  // create this texture.  So, we won't need |texture_owner| either.
  std::unique_ptr<AbstractTexture> texture = decoder_helper_->CreateTexture(
      GL_TEXTURE_EXTERNAL_OES, GL_RGBA, size.width(), size.height(), GL_RGBA,
      GL_UNSIGNED_BYTE);

  // Attach the image to the texture.
  // Either way, we expect this to be UNBOUND (i.e., decoder-managed).  For
  // overlays, BindTexImage will return true, causing it to transition to the
  // BOUND state, and thus receive ScheduleOverlayPlane calls.  For TextureOwner
  // backed images, BindTexImage will return false, and CopyTexImage will be
  // tried next.
  // TODO(liberato): consider not binding this as a StreamTextureImage if we're
  // using an overlay.  There's no advantage.  We'd likely want to create (and
  // initialize to a 1x1 texture) a 2D texture above in that case, in case
  // somebody tries to sample from it.  Be sure that promotion hints still
  // work properly, though -- they might require a stream texture image.
  GLuint texture_owner_service_id =
      texture_owner ? texture_owner->GetTextureId() : 0;
  texture->BindStreamTextureImage(image.get(), texture_owner_service_id);

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    DLOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    ContextStateResultUMA(result);
    return false;
  }

  // Create a shared image.
  // TODO(vikassoni): Hardcoding colorspace to SRGB. Figure how if media has a
  // colorspace and wire it here.
  // TODO(vikassoni): This shared image need to be thread safe eventually for
  // webview to work with shared images.
  auto shared_image = std::make_unique<gpu::SharedImageVideo>(
      mailbox, size, gfx::ColorSpace::CreateSRGB(), std::move(image),
      std::move(texture), std::move(shared_context),
      false /* is_thread_safe */);

  // Register it with shared image mailbox as well as legacy mailbox. This
  // keeps |shared_image| around until its destruction cb is called.
  // NOTE: Currently none of the video mailbox consumer uses shared image
  // mailbox.
  DCHECK(stub_->channel()->gpu_channel_manager()->shared_image_manager());
  stub_->channel()->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image), /* legacy_mailbox */ true);

  return true;
}

void GpuSharedImageVideoFactory::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
  decoder_helper_ = nullptr;
}

}  // namespace media
