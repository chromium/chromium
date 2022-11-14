// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/direct_shared_image_video_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
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
    GetStubCB get_stub_cb,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
      gpu_factory_(gpu_task_runner, std::move(get_stub_cb)),
      gpu_task_runner_(std::move(gpu_task_runner)) {}

DirectSharedImageVideoProvider::~DirectSharedImageVideoProvider() = default;

// TODO(liberato): add a thread hop to create the default texture owner, but
// not as part of this class.  just post something from VideoFrameFactory.
void DirectSharedImageVideoProvider::Initialize(GpuInitCB gpu_init_cb) {
  // Note that we do use not `AsyncCall()` + `Then()` to call `gpu_init_cb`,
  // since it is supposed to be called on the gpu main thread, which is somewhat
  // hacky.
  gpu_factory_.AsyncCall(&GpuSharedImageVideoFactory::Initialize)
      .WithArgs(std::move(gpu_init_cb));
}

void DirectSharedImageVideoProvider::RequestImage(ImageReadyCB cb,
                                                  const ImageSpec& spec) {
  // It's unclear that we should handle the image group, but since CodecImages
  // have to be registered on it, we do.  If the CodecImage is ever re-used,
  // then part of that re-use would be to call the (then mis-named)
  // destruction cb to remove it from the group.
  //
  // Also note that CodecImage shouldn't be the thing that's added to the
  // group anyway.  The thing that owns buffer management is all we really
  // care about, and that doesn't have anything to do with GLImage.

  // Note: `cb` is only run on successful creation, so this does not use
  // `AsyncCall()` + `Then()` to chain the callbacks.
  gpu_factory_.AsyncCall(&GpuSharedImageVideoFactory::CreateImage)
      .WithArgs(BindToCurrentLoop(std::move(cb)), spec, GetDrDcLock());
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
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Generate a shared image mailbox.
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();
  auto codec_image =
      base::MakeRefCounted<CodecImage>(spec.coded_size, drdc_lock);

  TRACE_EVENT0("media", "GpuSharedImageVideoFactory::CreateVideoFrame");

  if (!CreateImageInternal(spec, mailbox, codec_image, drdc_lock)) {
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
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(codec_image),
      std::move(drdc_lock));

  std::move(image_ready_cb).Run(std::move(record));
}

bool GpuSharedImageVideoFactory::CreateImageInternal(
    const SharedImageVideoProvider::ImageSpec& spec,
    gpu::Mailbox mailbox,
    scoped_refptr<CodecImage> image,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_))
    return false;

  gpu::gles2::ContextGroup* group = stub_->decoder_context()->GetContextGroup();
  if (!group)
    return false;

  const auto& coded_size = spec.coded_size;

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    DLOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    ContextStateResultUMA(result);
    return false;
  }

  // Create a shared image.
  // TODO(vikassoni): This shared image need to be thread safe eventually for
  // webview to work with shared images.
  auto shared_image = gpu::AndroidVideoImageBacking::Create(
      mailbox, coded_size, spec.color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, std::move(image), std::move(shared_context),
      std::move(drdc_lock));

  // Register it with shared image mailbox. This keeps |shared_image| around
  // until its destruction cb is called. NOTE: Currently none of the video
  // mailbox consumer uses shared image mailbox.
  DCHECK(stub_->channel()->gpu_channel_manager()->shared_image_manager());
  stub_->channel()->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image));

  return true;
}

void GpuSharedImageVideoFactory::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
}

}  // namespace media
