// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_texture_wrapper.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Return;
using ::testing::Values;

namespace media {

class MockVideoProcessorProxy : public VideoProcessorProxy {
 public:
  MockVideoProcessorProxy() : VideoProcessorProxy(nullptr, nullptr) {}

  D3D11Status Init(uint32_t width, uint32_t height) override {
    return MockInit(width, height);
  }

  HRESULT CreateVideoProcessorOutputView(
      ID3D11Texture2D* output_texture,
      D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* output_view_descriptor,
      ID3D11VideoProcessorOutputView** output_view) override {
    return MockCreateVideoProcessorOutputView();
  }

  HRESULT CreateVideoProcessorInputView(
      ID3D11Texture2D* input_texture,
      D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* input_view_descriptor,
      ID3D11VideoProcessorInputView** input_view) override {
    return MockCreateVideoProcessorInputView();
  }

  void SetStreamColorSpace(const gfx::ColorSpace& color_space) override {
    last_stream_color_space_ = color_space;
  }

  void SetOutputColorSpace(const gfx::ColorSpace& color_space) override {
    last_output_color_space_ = color_space;
  }

  HRESULT VideoProcessorBlt(ID3D11VideoProcessorOutputView* output_view,
                            UINT output_frameno,
                            UINT stream_count,
                            D3D11_VIDEO_PROCESSOR_STREAM* streams) override {
    return MockVideoProcessorBlt();
  }

  MOCK_METHOD2(MockInit, D3D11Status(uint32_t, uint32_t));
  MOCK_METHOD0(MockCreateVideoProcessorOutputView, HRESULT());
  MOCK_METHOD0(MockCreateVideoProcessorInputView, HRESULT());
  MOCK_METHOD0(MockVideoProcessorBlt, HRESULT());

  // Most recent arguments to SetStream/OutputColorSpace()/etc.
  std::optional<gfx::ColorSpace> last_stream_color_space_;
  std::optional<gfx::ColorSpace> last_output_color_space_;

 private:
  ~MockVideoProcessorProxy() override = default;
};

class MockTexture2DWrapper : public Texture2DWrapper {
 public:
  MockTexture2DWrapper() {}

  D3D11Status ProcessTexture(
      const gfx::ColorSpace& input_color_space,
      scoped_refptr<gpu::ClientSharedImage>& shared_image_dest) override {
    return MockProcessTexture();
  }

  D3D11Status Init(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
                   GetCommandBufferHelperCB get_helper_cb,
                   ComD3D11Texture2D in_texture,
                   size_t array_slice,
                   scoped_refptr<media::D3D11PictureBuffer> picture_buffer,
                   PictureBufferGPUResourceInitDoneCB
                       picture_buffer_gpu_resource_init_done_cb) override {
    gpu_task_runner_ = std::move(gpu_task_runner);
    return MockInit();
  }

  D3D11Status BeginSharedImageAccess() override {
    return MockBeginSharedImageAccess();
  }

  MOCK_METHOD0(MockInit, D3D11Status());
  MOCK_METHOD0(MockProcessTexture, D3D11Status());
  MOCK_METHOD0(MockBeginSharedImageAccess, D3D11Status());

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
};

CommandBufferHelperPtr UselessHelper() {
  return nullptr;
}

class D3D11CopyingTexture2DWrapperTest
    : public ::testing::TestWithParam<
          std::tuple<HRESULT, HRESULT, HRESULT, bool, bool, bool, bool>> {
 public:
#define FIELD(TYPE, NAME, INDEX) \
  TYPE Get##NAME() { return std::get<INDEX>(GetParam()); }
  FIELD(HRESULT, CreateVideoProcessorOutputView, 0)
  FIELD(HRESULT, CreateVideoProcessorInputView, 1)
  FIELD(HRESULT, VideoProcessorBlt, 2)
  FIELD(bool, ProcessorProxyInit, 3)
  FIELD(bool, TextureWrapperInit, 4)
  FIELD(bool, ProcessTexture, 5)
  FIELD(bool, PassthroughColorSpace, 6)
#undef FIELD

  void SetUp() override {
    gpu_task_runner_ = task_environment_.GetMainThreadTaskRunner();
  }

  scoped_refptr<MockVideoProcessorProxy> ExpectProcessorProxy() {
    auto result = base::MakeRefCounted<MockVideoProcessorProxy>();
    ON_CALL(*result.get(), MockInit(_, _))
        .WillByDefault(
            Return(GetProcessorProxyInit()
                       ? D3D11Status::Codes::kOk
                       : D3D11Status::Codes::kCreateVideoProcessorFailed));

    ON_CALL(*result.get(), MockCreateVideoProcessorOutputView())
        .WillByDefault(Return(GetCreateVideoProcessorOutputView()));

    ON_CALL(*result.get(), MockCreateVideoProcessorInputView())
        .WillByDefault(Return(GetCreateVideoProcessorInputView()));

    ON_CALL(*result.get(), MockVideoProcessorBlt())
        .WillByDefault(Return(GetVideoProcessorBlt()));

    return result;
  }

  std::unique_ptr<MockTexture2DWrapper> ExpectTextureWrapper() {
    auto result = std::make_unique<MockTexture2DWrapper>();

    ON_CALL(*result.get(), MockInit())
        .WillByDefault(
            Return(GetTextureWrapperInit()
                       ? D3D11Status::Codes::kOk
                       : D3D11Status::Codes::kCreateVideoProcessorFailed));

    ON_CALL(*result.get(), MockProcessTexture())
        .WillByDefault(Return(
            GetProcessTexture()
                ? D3D11Status::Codes::kOk
                : D3D11Status::Codes::kCreateVideoProcessorOutputViewFailed));

    return result;
  }

  GetCommandBufferHelperCB CreateMockHelperCB() {
    return base::BindRepeating(&UselessHelper);
  }

  bool InitSucceeds() {
    return GetProcessorProxyInit() && GetTextureWrapperInit();
  }

  bool ProcessTextureSucceeds() {
    return GetProcessTexture() &&
           SUCCEEDED(GetCreateVideoProcessorOutputView()) &&
           SUCCEEDED(GetCreateVideoProcessorInputView()) &&
           SUCCEEDED(GetVideoProcessorBlt());
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
};

INSTANTIATE_TEST_SUITE_P(CopyingTexture2DWrapperTest,
                         D3D11CopyingTexture2DWrapperTest,
                         Combine(Values(S_OK, E_FAIL),
                                 Values(S_OK, E_FAIL),
                                 Values(S_OK, E_FAIL),
                                 Bool(),
                                 Bool(),
                                 Bool(),
                                 Bool()));

// For ever potential return value combination for the D3D11VideoProcessor,
// make sure that any failures result in a total failure.
TEST_P(D3D11CopyingTexture2DWrapperTest,
       CopyingTextureWrapperProcessesCorrectly) {
  gfx::Size size;
  auto processor = ExpectProcessorProxy();
  MockVideoProcessorProxy* processor_raw = processor.get();
  auto texture_wrapper = ExpectTextureWrapper();
  MockTexture2DWrapper* texture_wrapper_raw = texture_wrapper.get();
  auto wrapper = std::make_unique<CopyingTexture2DWrapper>(
      size, std::move(texture_wrapper), processor, nullptr);

  // TODO: check |gpu_task_runner_|.

  scoped_refptr<gpu::ClientSharedImage> shared_image;
  gfx::ColorSpace input_color_space = gfx::ColorSpace::CreateSRGBLinear();
  EXPECT_EQ(
      wrapper
          ->Init(gpu_task_runner_, CreateMockHelperCB(),
                 /*texture=*/nullptr, /*array_slice=*/0,
                 /*picture_buffer=*/nullptr,
                 /*picture_buffer_gpu_resource_init_done_cb=*/base::DoNothing())
          .is_ok(),
      InitSucceeds());
  task_environment_.RunUntilIdle();
  if (GetProcessorProxyInit()) {
    EXPECT_EQ(texture_wrapper_raw->gpu_task_runner_, gpu_task_runner_);
  }
  EXPECT_EQ(wrapper->ProcessTexture(input_color_space, shared_image).is_ok(),
            ProcessTextureSucceeds());

  if (ProcessTextureSucceeds()) {
    // Also expect that the input and copy spaces were provided to the video
    // processor as the stream and output color spaces, respectively.
    EXPECT_TRUE(processor_raw->last_stream_color_space_);
    EXPECT_EQ(*processor_raw->last_stream_color_space_, input_color_space);
    EXPECT_TRUE(processor_raw->last_output_color_space_);
    EXPECT_EQ(*processor_raw->last_output_color_space_, input_color_space);
  }

  // TODO: verify that these aren't sent multiple times, unless they change.
}

}  // namespace media
