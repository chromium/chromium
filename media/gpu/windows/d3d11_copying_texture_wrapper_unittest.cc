// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"

#include <string.h>

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/d3d11_texture_wrapper.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"
#include "media/gpu/windows/d3d_picture_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_win.h"

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Return;
using ::testing::Values;

namespace media {

class MockVideoProcessorProxy : public VideoProcessorProxy {
 public:
  MockVideoProcessorProxy()
      : VideoProcessorProxy(MakeComPtr<D3D11VideoDeviceMock>(), nullptr) {}

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

  void SetStreamColorSpace(DXGI_COLOR_SPACE_TYPE color_space) override {
    last_stream_color_space_ = color_space;
  }

  void SetOutputColorSpace(DXGI_COLOR_SPACE_TYPE color_space) override {
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
  std::optional<DXGI_COLOR_SPACE_TYPE> last_stream_color_space_;
  std::optional<DXGI_COLOR_SPACE_TYPE> last_output_color_space_;

 private:
  ~MockVideoProcessorProxy() override = default;
};

class MockTexture2DWrapper : public Texture2DWrapper {
 public:
  MockTexture2DWrapper() {}

  D3D11Status ProcessTexture(
      scoped_refptr<gpu::ClientSharedImage>& shared_image_dest) override {
    return MockProcessTexture();
  }

  D3D11Status Init(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
                   GetCommandBufferHelperCB get_helper_cb,
                   ComD3D11Texture2D in_texture,
                   size_t array_slice,
                   scoped_refptr<media::D3DPictureBuffer> picture_buffer,
                   PictureBufferGPUResourceInitDoneCB
                       picture_buffer_gpu_resource_init_done_cb) override {
    gpu_task_runner_ = std::move(gpu_task_runner);
    return MockInit();
  }

  D3D11Status BeginSharedImageAccess() override {
    return MockBeginSharedImageAccess();
  }

  const gfx::Size& GetSize() const override { return size_; }

  MOCK_METHOD0(MockInit, D3D11Status());
  MOCK_METHOD0(MockProcessTexture, D3D11Status());
  MOCK_METHOD0(MockBeginSharedImageAccess, D3D11Status());

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  gfx::Size size_;
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
  gfx::ColorSpace input_color_space = gfx::ColorSpace::CreateSRGBLinear();
  gfx::ColorSpace output_color_space = gfx::ColorSpace::CreateHDR10();
  auto wrapper = std::make_unique<CopyingTexture2DWrapper>(
      size, input_color_space, output_color_space, std::move(texture_wrapper),
      processor, nullptr, gpu::GpuDriverBugWorkarounds());

  // TODO: check |gpu_task_runner_|.
  scoped_refptr<gpu::ClientSharedImage> shared_image;
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
  EXPECT_EQ(wrapper->ProcessTexture(shared_image).is_ok(),
            ProcessTextureSucceeds());

  if (InitSucceeds()) {
    // Also expect that the input and copy spaces were provided to the video
    // processor as the stream and output color spaces, respectively.
    EXPECT_TRUE(processor_raw->last_stream_color_space_);
    EXPECT_EQ(*processor_raw->last_stream_color_space_,
              gfx::ColorSpaceWin::GetDXGIColorSpace(input_color_space));
    EXPECT_TRUE(processor_raw->last_output_color_space_);
    EXPECT_EQ(*processor_raw->last_output_color_space_,
              gfx::ColorSpaceWin::GetDXGIColorSpace(output_color_space));
  }

  // TODO: verify that these aren't sent multiple times, unless they change.
}

class CopyingTexture2DWrapperColorSpaceTest : public ::testing::Test {
 public:
  enum class DxgiExpectation { kG22TopLeft, kFromColorSpaces };

  void SetUp() override {
    gpu_task_runner_ = task_environment_.GetMainThreadTaskRunner();
  }

  void ExpectVideoProcessorDxgiColorSpaces(
      const gfx::ColorSpace& input_color_space,
      const gfx::ColorSpace& output_color_space,
      DXGI_FORMAT input_format,
      DXGI_FORMAT output_format,
      bool enable_workaround,
      DxgiExpectation expected) {
    SCOPED_TRACE(::testing::Message()
                 << input_color_space.ToString() << " -> "
                 << output_color_space.ToString()
                 << " enable_workaround=" << enable_workaround << " expected="
                 << (expected == DxgiExpectation::kG22TopLeft
                         ? "kG22TopLeft"
                         : "kFromColorSpaces"));
    gpu::GpuDriverBugWorkarounds workarounds;
    workarounds.use_g22_topleft_color_space_for_yuv_vp = enable_workaround;
    auto processor = MakeProcessor();
    MockVideoProcessorProxy* const processor_raw = processor.get();
    auto wrapper = std::make_unique<CopyingTexture2DWrapper>(
        gfx::Size(), input_color_space, output_color_space,
        MakeTextureWrapper(), processor, MakeTexture(output_format),
        workarounds);
    ASSERT_TRUE(wrapper
                    ->Init(gpu_task_runner_,
                           base::BindRepeating(&UselessHelper),
                           MakeTexture(input_format), /*array_slice=*/0,
                           /*picture_buffer=*/nullptr,
                           /*picture_buffer_gpu_resource_init_done_cb=*/
                           base::DoNothing())
                    .is_ok());
    ASSERT_TRUE(processor_raw->last_stream_color_space_);
    ASSERT_TRUE(processor_raw->last_output_color_space_);
    DXGI_COLOR_SPACE_TYPE expected_stream;
    DXGI_COLOR_SPACE_TYPE expected_output;
    switch (expected) {
      case DxgiExpectation::kG22TopLeft:
        expected_stream = expected_output =
            DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020;
        break;
      case DxgiExpectation::kFromColorSpaces:
        expected_stream =
            gfx::ColorSpaceWin::GetDXGIColorSpace(input_color_space);
        expected_output =
            gfx::ColorSpaceWin::GetDXGIColorSpace(output_color_space);
        break;
    }
    EXPECT_EQ(*processor_raw->last_stream_color_space_, expected_stream);
    EXPECT_EQ(*processor_raw->last_output_color_space_, expected_output);
  }

 private:
  scoped_refptr<MockVideoProcessorProxy> MakeProcessor() {
    auto processor = base::MakeRefCounted<MockVideoProcessorProxy>();
    ON_CALL(*processor, MockInit(_, _))
        .WillByDefault(Return(D3D11Status::Codes::kOk));
    return processor;
  }

  std::unique_ptr<MockTexture2DWrapper> MakeTextureWrapper() {
    auto wrapper = std::make_unique<MockTexture2DWrapper>();
    ON_CALL(*wrapper, MockInit())
        .WillByDefault(Return(D3D11Status::Codes::kOk));
    return wrapper;
  }

  ComD3D11Texture2D MakeTexture(DXGI_FORMAT format) {
    auto texture = MakeComPtr<D3D11Texture2DMock>();
    ON_CALL(*texture.Get(), GetDesc(_))
        .WillByDefault([format](D3D11_TEXTURE2D_DESC* desc) {
          *desc = {};
          desc->Format = format;
        });
    return texture;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
};

TEST_F(CopyingTexture2DWrapperColorSpaceTest,
       SetsVideoProcessorDxgiColorSpaces) {
  using TransferID = gfx::ColorSpace::TransferID;
  using RangeID = gfx::ColorSpace::RangeID;
  auto bt2020_yuv = [](TransferID transfer, RangeID range) {
    return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020, transfer,
                           gfx::ColorSpace::MatrixID::BT2020_NCL, range);
  };
  const gfx::ColorSpace pq_limited_yuv =
      bt2020_yuv(TransferID::PQ, RangeID::LIMITED);
  const gfx::ColorSpace pq_full_yuv = bt2020_yuv(TransferID::PQ, RangeID::FULL);
  const gfx::ColorSpace bt2020_g22_limited_yuv =
      bt2020_yuv(TransferID::BT709, RangeID::LIMITED);
  const gfx::ColorSpace bt2020_g22_full_yuv =
      bt2020_yuv(TransferID::BT709, RangeID::FULL);
  const gfx::ColorSpace hlg_limited_yuv =
      bt2020_yuv(TransferID::HLG, RangeID::LIMITED);
  const gfx::ColorSpace rec709_yuv = gfx::ColorSpace::CreateREC709();
  const gfx::ColorSpace hdr10_rgb = gfx::ColorSpace::CreateHDR10();

  // NVIDIA zeros Y416->P010 identity copies tagged PQ (G2084).
  ExpectVideoProcessorDxgiColorSpaces(
      pq_limited_yuv, pq_limited_yuv, DXGI_FORMAT_Y416, DXGI_FORMAT_P010,
      /*enable_workaround=*/true, DxgiExpectation::kG22TopLeft);

  // FULL_G22_LEFT identity copies are also zeroed.
  ExpectVideoProcessorDxgiColorSpaces(bt2020_g22_full_yuv, bt2020_g22_full_yuv,
                                      DXGI_FORMAT_Y416, DXGI_FORMAT_P010,
                                      /*enable_workaround=*/true,
                                      DxgiExpectation::kG22TopLeft);

  // PQ limited and full differ as ColorSpaces but share a DXGI tag.
  ExpectVideoProcessorDxgiColorSpaces(
      pq_limited_yuv, pq_full_yuv, DXGI_FORMAT_Y416, DXGI_FORMAT_P010,
      /*enable_workaround=*/true, DxgiExpectation::kG22TopLeft);

  // With the flag off, PQ identity keeps the G2084 tags.
  ExpectVideoProcessorDxgiColorSpaces(
      pq_limited_yuv, pq_limited_yuv, DXGI_FORMAT_Y416, DXGI_FORMAT_P010,
      /*enable_workaround=*/false, DxgiExpectation::kFromColorSpaces);

  // YUV->RGB is not a YUV copy.
  ExpectVideoProcessorDxgiColorSpaces(
      pq_limited_yuv, hdr10_rgb, DXGI_FORMAT_Y416,
      DXGI_FORMAT_R10G10B10A2_UNORM, /*enable_workaround=*/true,
      DxgiExpectation::kFromColorSpaces);

  // Mismatched DXGI tags are a conversion, not a copy.
  ExpectVideoProcessorDxgiColorSpaces(
      pq_limited_yuv, rec709_yuv, DXGI_FORMAT_Y416, DXGI_FORMAT_P010,
      /*enable_workaround=*/true, DxgiExpectation::kFromColorSpaces);

  // PQ->G22 TOPLEFT is a conversion, not the identity-copy workaround.
  ExpectVideoProcessorDxgiColorSpaces(pq_limited_yuv, bt2020_g22_limited_yuv,
                                      DXGI_FORMAT_Y416, DXGI_FORMAT_P010,
                                      /*enable_workaround=*/true,
                                      DxgiExpectation::kFromColorSpaces);

  // HLG identity already uses a working DXGI tag.
  ExpectVideoProcessorDxgiColorSpaces(
      hlg_limited_yuv, hlg_limited_yuv, DXGI_FORMAT_Y416, DXGI_FORMAT_P010,
      /*enable_workaround=*/true, DxgiExpectation::kFromColorSpaces);

  // Rec709 NV12 is the 8-bit identity path.
  ExpectVideoProcessorDxgiColorSpaces(
      rec709_yuv, rec709_yuv, DXGI_FORMAT_NV12, DXGI_FORMAT_NV12,
      /*enable_workaround=*/true, DxgiExpectation::kFromColorSpaces);
}

}  // namespace media
