// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/frame_info_helper.h"

#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/shared_image_video.h"
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
class FrameInfoHelperImpl : public FrameInfoHelper {
 public:
  FrameInfoHelperImpl(scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
                      SharedImageVideoProvider::GetStubCB get_stub_cb) {
    on_gpu_ = base::SequenceBound<OnGpu>(std::move(gpu_task_runner),
                                         std::move(get_stub_cb));
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

  class OnGpu : public gpu::CommandBufferStub::DestructionObserver {
   public:
    OnGpu(SharedImageVideoProvider::GetStubCB get_stub_cb) {
      stub_ = get_stub_cb.Run();
      if (stub_)
        stub_->AddDestructionObserver(this);
    }

    ~OnGpu() override {
      if (stub_)
        stub_->RemoveDestructionObserver(this);
    }

    void OnWillDestroyStub(bool have_context) override {
      DCHECK(stub_);
      stub_ = nullptr;
    }

    void GetFrameInfoImpl(
        std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
        base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                base::Optional<FrameInfo>)> cb) {
      DCHECK(buffer_renderer);

      auto texture_owner = buffer_renderer->texture_owner();
      DCHECK(texture_owner);

      base::Optional<FrameInfo> info;

      if (buffer_renderer->RenderToTextureOwnerFrontBuffer(
              CodecOutputBufferRenderer::BindingsMode::kDontRestoreIfBound)) {
        gfx::Size coded_size;
        gfx::Rect visible_rect;
        if (texture_owner->GetCodedSizeAndVisibleRect(
                buffer_renderer->size(), &coded_size, &visible_rect)) {
          info.emplace();
          info->coded_size = coded_size;
          info->visible_rect = visible_rect;
          info->ycbcr_info = GetYCbCrInfo(texture_owner.get());
        }
      }

      std::move(cb).Run(std::move(buffer_renderer), info);
    }

    void GetFrameInfo(
        std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
        base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                                base::Optional<FrameInfo>)> cb) {
      DCHECK(buffer_renderer);

      auto texture_owner = buffer_renderer->texture_owner();
      DCHECK(texture_owner);

      auto buffer_available_cb =
          base::BindOnce(&OnGpu::GetFrameInfoImpl, weak_factory_.GetWeakPtr(),
                         std::move(buffer_renderer), std::move(cb));
      texture_owner->RunWhenBufferIsAvailable(std::move(buffer_available_cb));
    }

   private:
    // Gets YCbCrInfo from last rendered frame.
    base::Optional<gpu::VulkanYCbCrInfo> GetYCbCrInfo(
        gpu::TextureOwner* texture_owner) {
      gpu::ContextResult result;

      if (!stub_)
        return base::nullopt;

      auto shared_context =
          stub_->channel()->gpu_channel_manager()->GetSharedContextState(
              &result);
      auto context_provider =
          (result == gpu::ContextResult::kSuccess) ? shared_context : nullptr;
      if (!context_provider)
        return base::nullopt;

      return gpu::SharedImageVideo::GetYcbcrInfo(texture_owner,
                                                 context_provider);
    }

    gpu::CommandBufferStub* stub_ = nullptr;
    base::WeakPtrFactory<OnGpu> weak_factory_{this};
  };

  FrameInfo GetFrameInfoWithVisibleSize(const gfx::Size& visible_size) {
    FrameInfo info;
    info.coded_size = visible_size;
    info.visible_rect = gfx::Rect(visible_size);
    return info;
  }

  void OnFrameInfoReady(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      base::Optional<FrameInfo> frame_info) {
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
    SharedImageVideoProvider::GetStubCB get_stub_cb) {
  return std::make_unique<FrameInfoHelperImpl>(std::move(gpu_task_runner),
                                               std::move(get_stub_cb));
}

}  // namespace media
