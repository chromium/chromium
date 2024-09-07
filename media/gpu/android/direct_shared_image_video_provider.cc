// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/direct_shared_image_video_provider.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"
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
    gpu::CommandBufferStub* stub) {
  gpu::ContextResult result;
  auto shared_context =
      stub->channel()->gpu_channel_manager()->GetSharedContextState(&result);
  return (result == gpu::ContextResult::kSuccess) ? shared_context : nullptr;
}

}  // namespace

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
  // care about.

  // Note: `cb` is only run on successful creation, so this does not use
  // `AsyncCall()` + `Then()` to chain the callbacks.
  gpu_factory_.AsyncCall(&GpuSharedImageVideoFactory::CreateImage)
      .WithArgs(base::BindPostTaskToCurrentDefault(std::move(cb)), spec,
                GetDrDcLock());
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

  auto shared_context = GetSharedContext(stub_);
  if (!shared_context) {
    DLOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    std::move(gpu_init_cb).Run(nullptr);
    return;
  }

  is_vulkan_ = shared_context->GrContextIsVulkan();

  // Make the shared context current.
  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      shared_context->context(), shared_context->surface());
  if (!shared_context->IsCurrent(nullptr)) {
    DLOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to make shared context current.";
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

  if (!stub_) {
    return;
  }

  auto codec_image =
      base::MakeRefCounted<CodecImage>(spec.coded_size, drdc_lock);

  TRACE_EVENT0("media", "GpuSharedImageVideoFactory::CreateVideoFrame");

  scoped_refptr<gpu::GpuChannelSharedImageInterface>
      gpu_channel_shared_image_interface =
          stub_->channel()->shared_image_stub()->shared_image_interface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu_channel_shared_image_interface->CreateSharedImageForAndroidVideo(
          spec.coded_size, spec.color_space, codec_image, drdc_lock);
  if (!shared_image) {
    return;
  }

  SharedImageVideoProvider::ImageRecord record;
  record.shared_image = std::move(shared_image);
  record.is_vulkan = is_vulkan_;

  // Since |codec_image|'s ref holders can be destroyed by stub destruction,
  // we create a ref to it for the MaybeRenderEarlyManager.  This is a hack;
  // we should not be sending the CodecImage at all.  The
  // MaybeRenderEarlyManager should work with some other object that happens
  // to be used by CodecImage, and non-GL things, to hold the output buffer,
  // etc.
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

  const auto& coded_size = spec.coded_size;

  auto shared_context = GetSharedContext(stub_);
  if (!shared_context) {
    DLOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    return false;
  }

  // Create a shared image.
  // TODO(vikassoni): This shared image need to be thread safe eventually for
  // webview to work with shared images.
  auto shared_image = gpu::AndroidVideoImageBacking::Create(
      mailbox, coded_size, spec.color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, /*debug_label=*/"DirectSIVideo", std::move(image),
      std::move(shared_context), std::move(drdc_lock));

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
