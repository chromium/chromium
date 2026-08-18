// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_processor_wrapper.h"

#include <memory>

#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/d3d12_video_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace media {

class D3D12VideoProcessorWrapperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>();
    video_device_ = MakeComPtr<NiceMock<D3D12VideoDevice3Mock>>();
    command_queue_ = MakeComPtr<NiceMock<D3D12CommandQueueMock>>();
    command_allocator_ = MakeComPtr<NiceMock<D3D12CommandAllocatorMock>>();
    command_list_ = MakeComPtr<NiceMock<D3D12VideoProcessCommandListMock>>();
    video_processor_ = MakeComPtr<NiceMock<D3D12VideoProcessorMock>>();
    fence_ = MakeComPtr<NiceMock<D3D12FenceMock>>();

    ON_CALL(*video_device_.Get(), QueryInterface(IID_ID3D12Device, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(device_.Get()));
    ON_CALL(*device_.Get(), CreateCommandQueue(_, _, _))
        .WillByDefault(SetComPointeeAndReturnOk<2>(command_queue_.Get()));
    ON_CALL(*device_.Get(), CreateCommandAllocator(_, _, _))
        .WillByDefault(SetComPointeeAndReturnOk<2>(command_allocator_.Get()));
    ON_CALL(*device_.Get(), CreateCommandList(_, _, _, _, _, _))
        .WillByDefault(SetComPointeeAndReturnOk<5>(command_list_.Get()));
    ON_CALL(*device_.Get(), CreateFence(_, _, _, _))
        .WillByDefault(SetComPointeeAndReturnOk<3>(fence_.Get()));
    ON_CALL(*command_list_.Get(), Close()).WillByDefault(Return(S_OK));
    ON_CALL(*command_list_.Get(), Reset(_)).WillByDefault(Return(S_OK));
    ON_CALL(*command_allocator_.Get(), Reset()).WillByDefault(Return(S_OK));
    ON_CALL(*command_queue_.Get(), Signal(_, _)).WillByDefault(Return(S_OK));
    ON_CALL(*fence_.Get(), GetCompletedValue()).WillByDefault(Return(0));
    ON_CALL(*fence_.Get(), SetEventOnCompletion(_, _))
        .WillByDefault([](UINT64, HANDLE event) {
          ::SetEvent(event);
          return S_OK;
        });
    ON_CALL(*video_device_.Get(),
            CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_SUPPORT, _, _))
        .WillByDefault([](D3D12_FEATURE_VIDEO, void* data, UINT) {
          static_cast<D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT*>(data)
              ->SupportFlags = D3D12_VIDEO_PROCESS_SUPPORT_FLAG_SUPPORTED;
          return S_OK;
        });
    ON_CALL(*video_device_.Get(), CreateVideoProcessor(_, _, _, _, _, _))
        .WillByDefault(SetComPointeeAndReturnOk<5>(video_processor_.Get()));

    wrapper_ = std::make_unique<D3D12VideoProcessorWrapper>(video_device_);
  }

  Microsoft::WRL::ComPtr<ID3D12Resource> CreateInputResource(
      const gfx::Size& size = gfx::Size(1280, 720)) {
    auto resource = MakeComPtr<NiceMock<D3D12ResourceMock>>();
    ON_CALL(*resource.Get(), GetDesc())
        .WillByDefault(Return(D3D12_RESOURCE_DESC{
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = static_cast<UINT64>(size.width()),
            .Height = static_cast<UINT>(size.height()),
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_NV12,
        }));
    return resource;
  }

  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12VideoDevice3Mock> video_device_;
  Microsoft::WRL::ComPtr<D3D12CommandQueueMock> command_queue_;
  Microsoft::WRL::ComPtr<D3D12CommandAllocatorMock> command_allocator_;
  Microsoft::WRL::ComPtr<D3D12VideoProcessCommandListMock> command_list_;
  Microsoft::WRL::ComPtr<D3D12VideoProcessorMock> video_processor_;
  Microsoft::WRL::ComPtr<D3D12FenceMock> fence_;

  std::unique_ptr<D3D12VideoProcessorWrapper> wrapper_;
};

TEST_F(D3D12VideoProcessorWrapperTest, DestructorWaitsForPendingWork) {
  ASSERT_TRUE(wrapper_->Init());

  auto input = CreateInputResource();
  auto output = CreateInputResource();
  gfx::Rect rect(0, 0, 1280, 720);
  EXPECT_CALL(*command_queue_.Get(), ExecuteCommandLists(1, _));
  auto fence_and_value = wrapper_->ProcessFrames(
      input.Get(), 0, gfx::ColorSpace::CreateSRGB(), rect, output.Get(), 0,
      gfx::ColorSpace::CreateREC709(), rect);
  ASSERT_TRUE(fence_and_value.first);
  EXPECT_EQ(fence_and_value.second, 1u);

  // The work submitted above has not yet completed on the GPU. Destroying the
  // wrapper must block until it does so that resources referenced by the
  // command list are not released early.
  EXPECT_CALL(*fence_.Get(), SetEventOnCompletion(1, _))
      .WillOnce([](UINT64, HANDLE event) {
        ::SetEvent(event);
        return S_OK;
      });
  wrapper_.reset();
}

TEST_F(D3D12VideoProcessorWrapperTest,
       DestructorDoesNotWaitWithoutPendingWork) {
  ASSERT_TRUE(wrapper_->Init());

  EXPECT_CALL(*fence_.Get(), SetEventOnCompletion(_, _)).Times(0);
  wrapper_.reset();
}

TEST_F(D3D12VideoProcessorWrapperTest, CropDrivesSourceSizeRangeAndSourceRect) {
  ASSERT_TRUE(wrapper_->Init());

  auto input = CreateInputResource(gfx::Size(1920, 1088));
  auto output = CreateInputResource(gfx::Size(1920, 1080));
  const gfx::Rect crop(0, 0, 1920, 1080);

  D3D12_VIDEO_SIZE_RANGE source_size_range{};
  EXPECT_CALL(*video_device_.Get(), CreateVideoProcessor(_, _, 1, _, _, _))
      .WillOnce([&](UINT, const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC*, UINT,
                    const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC* input_descs,
                    REFIID, void** out) {
        source_size_range = input_descs[0].SourceSizeRange;
        *out = video_processor_.Get();
        video_processor_->AddRef();
        return S_OK;
      });
  RECT source_rect{};
  EXPECT_CALL(*command_list_.Get(), ProcessFrames(_, _, 1, _))
      .WillOnce([&](ID3D12VideoProcessor*,
                    const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS*, UINT,
                    const D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS* args) {
        source_rect = args[0].Transform.SourceRectangle;
      });

  ASSERT_TRUE(wrapper_
                  ->ProcessFrames(input.Get(), 0, gfx::ColorSpace::CreateSRGB(),
                                  crop, output.Get(), 0,
                                  gfx::ColorSpace::CreateREC709(),
                                  gfx::Rect(0, 0, 1920, 1080))
                  .first);

  // The minimum is the crop; the maximum stays at the texture size.
  EXPECT_EQ(source_size_range.MinWidth, 1920u);
  EXPECT_EQ(source_size_range.MinHeight, 1080u);
  EXPECT_EQ(source_size_range.MaxWidth, 1920u);
  EXPECT_EQ(source_size_range.MaxHeight, 1088u);

  EXPECT_EQ(gfx::Rect(source_rect.left, source_rect.top,
                      source_rect.right - source_rect.left,
                      source_rect.bottom - source_rect.top),
            crop);
}

TEST_F(D3D12VideoProcessorWrapperTest, SupportQueryUsesCropSize) {
  ASSERT_TRUE(wrapper_->Init());

  auto input = CreateInputResource(gfx::Size(1920, 1088));
  auto output = CreateInputResource(gfx::Size(1280, 720));

  D3D12_VIDEO_SAMPLE input_sample{};
  EXPECT_CALL(*video_device_.Get(),
              CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_SUPPORT, _, _))
      .WillOnce([&](D3D12_FEATURE_VIDEO, void* data, UINT) {
        auto* support =
            static_cast<D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT*>(data);
        input_sample = support->InputSample;
        support->SupportFlags = D3D12_VIDEO_PROCESS_SUPPORT_FLAG_SUPPORTED;
        return S_OK;
      });

  ASSERT_TRUE(wrapper_
                  ->ProcessFrames(input.Get(), 0, gfx::ColorSpace::CreateSRGB(),
                                  gfx::Rect(16, 8, 1600, 900), output.Get(), 0,
                                  gfx::ColorSpace::CreateREC709(),
                                  gfx::Rect(0, 0, 1280, 720))
                  .first);

  EXPECT_EQ(input_sample.Width, 1600u);
  EXPECT_EQ(input_sample.Height, 900u);
}

TEST_F(D3D12VideoProcessorWrapperTest, ProcessorRecreatedOnlyWhenCropChanges) {
  ASSERT_TRUE(wrapper_->Init());

  auto input = CreateInputResource(gfx::Size(1920, 1088));
  auto output = CreateInputResource(gfx::Size(1280, 720));
  const gfx::Rect destination(0, 0, 1280, 720);
  auto process = [&](const gfx::Rect& crop) {
    return wrapper_
        ->ProcessFrames(input.Get(), 0, gfx::ColorSpace::CreateSRGB(), crop,
                        output.Get(), 0, gfx::ColorSpace::CreateREC709(),
                        destination)
        .first;
  };

  EXPECT_CALL(*video_device_.Get(), CreateVideoProcessor(_, _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(SetComPointeeAndReturnOk<5>(video_processor_.Get()));

  ASSERT_TRUE(process(gfx::Rect(0, 0, 1920, 1080)));
  // Same crop: the cached processor is reused.
  ASSERT_TRUE(process(gfx::Rect(0, 0, 1920, 1080)));
  // Different crop: the processor is recreated.
  ASSERT_TRUE(process(gfx::Rect(0, 0, 1600, 900)));
}

}  // namespace media
