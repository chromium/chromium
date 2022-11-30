// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/frame_info_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/android/codec_output_buffer_renderer.h"

namespace media {

FrameInfoHelper::FrameInfo::FrameInfo() = default;
FrameInfoHelper::FrameInfo::~FrameInfo() = default;
FrameInfoHelper::FrameInfo::FrameInfo(FrameInfo&&) = default;
FrameInfoHelper::FrameInfo::FrameInfo(const FrameInfoHelper::FrameInfo&) =
    default;
FrameInfoHelper::FrameInfo& FrameInfoHelper::FrameInfo::operator=(
    const FrameInfoHelper::FrameInfo&) = default;

// Concrete implementation of FrameInfoHelper that renders output buffers and
// gets the FrameInfo they need.
class FrameInfoHelperImpl : public FrameInfoHelper,
                            public gpu::RefCountedLockHelperDrDc {
 public:
  FrameInfoHelperImpl(scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
                      SharedImageVideoProvider::GetStubCB get_stub_cb,
                      scoped_refptr<gpu::RefCountedLock> drdc_lock)
      : gpu::RefCountedLockHelperDrDc(drdc_lock) {
    on_gpu_ = base::SequenceBound<OnGpu>(std::move(gpu_task_runner),
                                         std::move(get_stub_cb),
                                         std::move(drdc_lock));
  }

  ~FrameInfoHelperImpl() override = default;

  void GetFrameInfo(std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
                    FrameInfoReadyCB callback) override {
    Request request = {.buffer_renderer = std::move(buffer_renderer),
                       .callback = std::move(callback)};
    requests_.push(std::move(request));
    // If there were no pending requests start processing queue now.
    if (requests_.size() == 1)
      ProcessRequestsQueue();
  }

 private:
  struct Request {
    std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer;
    FrameInfoReadyCB callback;
  };

  class OnGpu : public gpu::RefCountedLockHelperDrDc {
   public:
    OnGpu(SharedImageVideoProvider::GetStubCB get_stub_cb,
          scoped_refptr<gpu::RefCountedLock> drdc_lock)
        : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
          frame_info_helper_holder_(
              base::MakeRefCounted<FrameInfoHelperHolder>(this)) {
      auto* stub = get_stub_cb.Run();
      if (stub) {
        gpu::ContextResult result;
        shared_context_ =
            stub->channel()->gpu_channel_manager()->GetSharedContextState(
                &result);
        if (result == gpu::ContextResult::kSuccess) {
          DCHECK(shared_context_);
          if (shared_context_->GrContextIsVulkan()) {
            vulkan_context_provider_ = shared_context_->vk_context_provider();
          }
        }
      }
    }

    ~OnGpu() {
      DCHECK(frame_info_helper_holder_);
      frame_info_helper_holder_->SetFrameInfoHelperOnGpuToNull();
    }

    void GetFrameInfoImpl(
        std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
        base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                absl::optional<FrameInfo>)> cb) {
      AssertAcquiredDrDcLock();
      DCHECK(buffer_renderer);

      auto texture_owner = buffer_renderer->texture_owner();
      DCHECK(texture_owner);

      absl::optional<FrameInfo> info;

      if (buffer_renderer->RenderToTextureOwnerFrontBuffer(
              CodecOutputBufferRenderer::BindingsMode::kDontBindImage, 0)) {
        gfx::Size coded_size;
        gfx::Rect visible_rect;
        if (texture_owner->GetCodedSizeAndVisibleRect(
                buffer_renderer->size(), &coded_size, &visible_rect)) {
          info.emplace();
          info->coded_size = coded_size;
          info->visible_rect = visible_rect;
          info->ycbcr_info = gpu::AndroidVideoImageBacking::GetYcbcrInfo(
              texture_owner.get(), vulkan_context_provider_);
        }
      }
      std::move(cb).Run(std::move(buffer_renderer), info);
    }

    void GetFrameInfo(
        std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
        base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                absl::optional<FrameInfo>)> cb) {
      // Note that we need to ensure that no other thread renders another buffer
      // in between while we are getting frame info here. Otherwise we will get
      // wrong frame info. This is ensured by holding |drdc_lock| from all the
      // places from where GetFrameInfoImpl() call can be triggered. It can be
      // called from here via |texture_owner->RunWhenBufferIsAvailable()| below
      // or from |ImageReaderGLOwner::ReleaseRefOnImageLocked()| when the
      // buffer_vailable_cb is cached and triggered. So we lock here as well as
      // ensure lock is held during
      // |ImageReaderGLOwner::ReleaseRefOnImageLocked|.
      base::AutoLockMaybe auto_lock(GetDrDcLockPtr());
      DCHECK(buffer_renderer);

      auto texture_owner = buffer_renderer->texture_owner();
      DCHECK(texture_owner);

      auto buffer_available_cb =
          base::BindOnce(&FrameInfoHelperHolder::GetFrameInfoImpl,
                         base::RetainedRef(frame_info_helper_holder_),
                         std::move(buffer_renderer), std::move(cb));
      texture_owner->RunWhenBufferIsAvailable(std::move(buffer_available_cb));
    }

   private:
    // OnGpu::GetFrameInfoImpl can be called from any gpu thread (gpu main or
    // DrDc), hence we can not use WeakPtr to it in |buffer_available_cb|.
    // FrameInfoHelperHolder is used instead to mimic this weakPtr behavior of
    // OnGpu. FrameInfoHelperHolder is RefCountedThreadSafe, and has a pointer
    // to the OnGpu. OnGpu owns the FrameInfoHelperHolder and sets this pointer
    // to null in its destructor so that it cant be used once OnGpu is
    // destroyed. Note that since OnGpu::GetFrameInfoImpl needed to be called
    // from any gpu thread, we could not use WeakPtr to it.
    class FrameInfoHelperHolder
        : public base::RefCountedThreadSafe<FrameInfoHelperHolder> {
     public:
      explicit FrameInfoHelperHolder(raw_ptr<OnGpu> frame_info_helper_on_gpu)
          : frame_info_helper_on_gpu_(frame_info_helper_on_gpu) {
        DCHECK(frame_info_helper_on_gpu_);
      }

      void GetFrameInfoImpl(
          std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
          base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                  absl::optional<FrameInfo>)> cb) {
        base::AutoLock l(lock_);
        if (frame_info_helper_on_gpu_) {
          frame_info_helper_on_gpu_->GetFrameInfoImpl(
              std::move(buffer_renderer), std::move(cb));
        }
      }

      void SetFrameInfoHelperOnGpuToNull() {
        base::AutoLock l(lock_);
        frame_info_helper_on_gpu_ = nullptr;
      }

     private:
      friend class base::RefCountedThreadSafe<FrameInfoHelperHolder>;
      ~FrameInfoHelperHolder() = default;

      // |lock_| for thread safe access to |frame_info_helper_on_gpu_|.
      base::Lock lock_;
      raw_ptr<OnGpu> frame_info_helper_on_gpu_ GUARDED_BY(lock_) = nullptr;
    };

    // Note that |shared_context_| is to just keep ref on it until
    // |vulkan_context_provider_| raw_ptr is being used.
    scoped_refptr<gpu::SharedContextState> shared_context_;
    raw_ptr<viz::VulkanContextProvider> vulkan_context_provider_ = nullptr;
    scoped_refptr<FrameInfoHelperHolder> frame_info_helper_holder_;
  };

  FrameInfo GetFrameInfoWithVisibleSize(const gfx::Size& visible_size) {
    FrameInfo info;
    info.coded_size = visible_size;
    info.visible_rect = gfx::Rect(visible_size);
    return info;
  }

  void OnFrameInfoReady(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      absl::optional<FrameInfo> frame_info) {
    DCHECK(buffer_renderer);
    DCHECK(!requests_.empty());

    auto& request = requests_.front();

    if (frame_info) {
      visible_size_ = buffer_renderer->size();
      frame_info_ = *frame_info;
      std::move(request.callback).Run(std::move(buffer_renderer), frame_info_);
    } else {
      // It's possible that we will fail to render frame and so weren't able to
      // obtain FrameInfo. In this case we don't cache new values and complete
      // current request with visible size, we will attempt to render next frame
      // with next request.
      auto info = GetFrameInfoWithVisibleSize(buffer_renderer->size());
      std::move(request.callback)
          .Run(std::move(buffer_renderer), std::move(info));
    }
    requests_.pop();
    ProcessRequestsQueue();
  }

  void ProcessRequestsQueue() {
    while (!requests_.empty()) {
      auto& request = requests_.front();
      if (!request.buffer_renderer) {
        // If we don't have buffer_renderer we can Run callback immediately.
        std::move(request.callback).Run(nullptr, FrameInfo());
      } else if (!request.buffer_renderer->texture_owner()) {
        // If there is no texture_owner (SurfaceView case), we can't render
        // frame and get proper size. But as Display Compositor won't render
        // this frame the actual size is not important, assume coded_size =
        // visible_size.
        auto info =
            GetFrameInfoWithVisibleSize(request.buffer_renderer->size());
        std::move(request.callback)
            .Run(std::move(request.buffer_renderer), std::move(info));
      } else if (visible_size_ == request.buffer_renderer->size()) {
        // We have cached the results of last frame info request with the same
        // size. We assume that coded_size doesn't change if the visible_size
        // stays the same.
        std::move(request.callback)
            .Run(std::move(request.buffer_renderer), frame_info_);
      } else {
        // We have texture_owner and we don't have cached value, so we need to
        // hop to GPU thread and render the frame to get proper size.
        auto cb = BindToCurrentLoop(
            base::BindOnce(&FrameInfoHelperImpl::OnFrameInfoReady,
                           weak_factory_.GetWeakPtr()));

        on_gpu_.AsyncCall(&OnGpu::GetFrameInfo)
            .WithArgs(std::move(request.buffer_renderer), std::move(cb));
        // We didn't complete this request quite yet, so we can't process queue
        // any further.
        break;
      }
      requests_.pop();
    }
  }

  base::SequenceBound<OnGpu> on_gpu_;
  std::queue<Request> requests_;

  // Cached values.
  FrameInfo frame_info_;
  gfx::Size visible_size_;

  base::WeakPtrFactory<FrameInfoHelperImpl> weak_factory_{this};
};

// static
std::unique_ptr<FrameInfoHelper> FrameInfoHelper::Create(
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    SharedImageVideoProvider::GetStubCB get_stub_cb,
    scoped_refptr<gpu::RefCountedLock> drdc_lock) {
  return std::make_unique<FrameInfoHelperImpl>(
      std::move(gpu_task_runner), std::move(get_stub_cb), std::move(drdc_lock));
}

}  // namespace media
