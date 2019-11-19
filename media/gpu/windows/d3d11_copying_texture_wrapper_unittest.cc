// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind_helpers.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
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

  bool Init(uint32_t width, uint32_t height) override {
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

  HRESULT VideoProcessorBlt(ID3D11VideoProcessorOutputView* output_view,
                            UINT output_frameno,
                            UINT stream_count,
                            D3D11_VIDEO_PROCESSOR_STREAM* streams) override {
    return MockVideoProcessorBlt();
  }

  MOCK_METHOD2(MockInit, bool(uint32_t, uint32_t));
  MOCK_METHOD0(MockCreateVideoProcessorOutputView, HRESULT());
  MOCK_METHOD0(MockCreateVideoProcessorInputView, HRESULT());
  MOCK_METHOD0(MockVideoProcessorBlt, HRESULT());
};

class MockTexture2DWrapper : public Texture2DWrapper {
 public:
  MockTexture2DWrapper() : Texture2DWrapper(nullptr) {}

  bool ProcessTexture(const D3D11PictureBuffer* owner_pb,
                      MailboxHolderArray* mailbox_dest) override {
    return MockProcessTexture();
  }

  bool Init(GetCommandBufferHelperCB get_helper_cb,
            size_t array_slice,
            gfx::Size size) override {
    return MockInit();
  }

  MOCK_METHOD0(MockInit, bool());
  MOCK_METHOD0(MockProcessTexture, bool());
};

CommandBufferHelperPtr UselessHelper() {
  return nullptr;
}

class D3D11CopyingTexture2DWrapperTest
    : public ::testing::TestWithParam<
          std::tuple<HRESULT, HRESULT, HRESULT, bool, bool, bool>> {
 public:
#define FIELD(TYPE, NAME, INDEX) \
  TYPE Get##NAME() { return std::get<INDEX>(GetParam()); }
  FIELD(HRESULT, CreateVideoProcessorOutputView, 0)
  FIELD(HRESULT, CreateVideoProcessorInputView, 1)
  FIELD(HRESULT, VideoProcessorBlt, 2)
  FIELD(bool, ProcessorProxyInit, 3)
  FIELD(bool, TextureWrapperInit, 4)
  FIELD(bool, ProcessTexture, 5)
#undef FIELD

  std::unique_ptr<VideoProcessorProxy> ExpectProcessorProxy() {
    auto result = std::make_unique<MockVideoProcessorProxy>();
    ON_CALL(*result.get(), MockInit(_, _))
        .WillByDefault(Return(GetProcessorProxyInit()));

    ON_CALL(*result.get(), MockCreateVideoProcessorOutputView())
        .WillByDefault(Return(GetCreateVideoProcessorOutputView()));

    ON_CALL(*result.get(), MockCreateVideoProcessorInputView())
        .WillByDefault(Return(GetCreateVideoProcessorInputView()));

    ON_CALL(*result.get(), MockVideoProcessorBlt())
        .WillByDefault(Return(GetVideoProcessorBlt()));

    return std::move(result);
  }

  std::unique_ptr<Texture2DWrapper> ExpectTextureWrapper() {
    auto result = std::make_unique<MockTexture2DWrapper>();

    ON_CALL(*result.get(), MockInit())
        .WillByDefault(Return(GetTextureWrapperInit()));

    ON_CALL(*result.get(), MockProcessTexture())
        .WillByDefault(Return(GetProcessTexture()));

    return std::move(result);
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
};

INSTANTIATE_TEST_CASE_P(CopyingTexture2DWrapperTest,
                        D3D11CopyingTexture2DWrapperTest,
                        Combine(Values(S_OK, E_FAIL),
                                Values(S_OK, E_FAIL),
                                Values(S_OK, E_FAIL),
                                Bool(),
                                Bool(),
                                Bool()));

// For ever potential return value combination for the D3D11VideoProcessor,
// make sure that any failures result in a total failure.
TEST_P(D3D11CopyingTexture2DWrapperTest,
       CopyingTextureWrapperProcessesCorrectly) {
  auto wrapper = std::make_unique<CopyingTexture2DWrapper>(
      ExpectTextureWrapper(), ExpectProcessorProxy(), nullptr);
  auto picture_buffer =
      base::MakeRefCounted<D3D11PictureBuffer>(nullptr, gfx::Size(0, 0), 0);

  EXPECT_EQ(wrapper->Init(CreateMockHelperCB(), 0, {}), InitSucceeds());
  EXPECT_EQ(wrapper->ProcessTexture(picture_buffer.get(), nullptr),
            ProcessTextureSucceeds());
}

}  // namespace media
