// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ycbcr_helper.h"

#include "gpu/command_buffer/service/shared_image_video.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"

namespace media {

// Concrete implementation of YCbCrHelper that renders output buffers and gets
// the YCbCrInfo they need.
class YCbCrHelperImpl : public YCbCrHelper,
                        public gpu::CommandBufferStub::DestructionObserver {
 public:
  YCbCrHelperImpl(SharedImageVideoProvider::GetStubCB get_stub_cb) {
    stub_ = get_stub_cb.Run();
    if (stub_)
      stub_->AddDestructionObserver(this);
  }

  ~YCbCrHelperImpl() override {
    if (stub_)
      stub_->RemoveDestructionObserver(this);
  }

  // YCbCrHelper
  void GetYCbCrInfo(
      scoped_refptr<CodecImageHolder> codec_image_holder,
      base::OnceCallback<void(OptionalInfo ycbcr_info)> cb) override {
    // If we don't have the info cached, then try to get it.  If we have gotten
    // it, then don't try again.  Assume that our caller asked for it before it
    // got the results back.  We don't want to render more frames to the front
    // buffer if we don't need to.
    if (!ycbcr_info_)
      ycbcr_info_ = RenderImageAndGetYCbCrInfo(std::move(codec_image_holder));

    // Whether we got it or not, send it along.
    std::move(cb).Run(ycbcr_info_);
  }

  void OnWillDestroyStub(bool have_context) override {
    DCHECK(stub_);
    stub_ = nullptr;
  }

 private:
  // Render the codec output buffer, and use it to get the YCbCrInfo.
  OptionalInfo RenderImageAndGetYCbCrInfo(
      scoped_refptr<CodecImageHolder> codec_image_holder) {
    gpu::ContextResult result;
    if (!stub_)
      return base::nullopt;

    auto shared_context =
        stub_->channel()->gpu_channel_manager()->GetSharedContextState(&result);
    auto context_provider =
        (result == gpu::ContextResult::kSuccess) ? shared_context : nullptr;
    if (!context_provider)
      return base::nullopt;

    return gpu::SharedImageVideo::GetYcbcrInfo(
        codec_image_holder->codec_image_raw(), context_provider);
  }

  gpu::CommandBufferStub* stub_ = nullptr;

  OptionalInfo ycbcr_info_;
};

// static
base::SequenceBound<YCbCrHelper> YCbCrHelper::Create(
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    SharedImageVideoProvider::GetStubCB get_stub_cb) {
  return base::SequenceBound<YCbCrHelperImpl>(std::move(gpu_task_runner),
                                              std::move(get_stub_cb));
}

}  // namespace media
