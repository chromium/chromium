// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <random>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "media/gpu/windows/d3d11_texture_wrapper.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace media {

using MockD3D11VideoDevice = Microsoft::WRL::ComPtr<D3D11VideoDeviceMock>;
using MockD3D11DeviceContext = Microsoft::WRL::ComPtr<D3D11DeviceContextMock>;
using MockD3D11VideoContext = Microsoft::WRL::ComPtr<D3D11VideoContextMock>;
using MockD3D11VideoProcessor = Microsoft::WRL::ComPtr<D3D11VideoProcessorMock>;
using MockD3D11VideoProcessorEnumerator =
    Microsoft::WRL::ComPtr<D3D11VideoProcessorEnumeratorMock>;

class D3D11VideoProcessorProxyUnittest : public ::testing::Test {
 public:
  MockD3D11VideoDevice dev_;
  MockD3D11DeviceContext ctx_;
  MockD3D11VideoContext vctx_;
  MockD3D11VideoProcessorEnumerator enumerator_;
  MockD3D11VideoProcessor proc_;

  scoped_refptr<VideoProcessorProxy> CreateProxy() {
    dev_ = MakeComPtr<D3D11VideoDeviceMock>();
    ctx_ = MakeComPtr<D3D11DeviceContextMock>();
    vctx_ = MakeComPtr<D3D11VideoContextMock>();
    proc_ = MakeComPtr<D3D11VideoProcessorMock>();
    enumerator_ = MakeComPtr<D3D11VideoProcessorEnumeratorMock>();

    EXPECT_CALL(*dev_.Get(), CreateVideoProcessorEnumerator(_, _))
        .WillOnce(SetComPointeeAndReturnOk<1>(enumerator_.Get()));

    EXPECT_CALL(*dev_.Get(), CreateVideoProcessor(_, _, _))
        .WillOnce(SetComPointeeAndReturnOk<2>(proc_.Get()));

    EXPECT_CALL(*ctx_.Get(), QueryInterface(_, _))
        .WillOnce(SetComPointeeAndReturnOk<1>(vctx_.Get()));

    return base::MakeRefCounted<VideoProcessorProxy>(dev_, ctx_);
  }

  // Pull a random pointer off the stack, rather than relying on nullptrs.
  template <typename T>
  T* CreateGarbagePtr() {
    int foo;
    void* local = &foo;
    return static_cast<T*>(local);
  }
};

// The processor proxy wraps the VideoDevice/VideoContext and stores some of the
// d3d11 types. Make sure that the arguments we give these methods are passed
// through correctly.
TEST_F(D3D11VideoProcessorProxyUnittest, EnsureMethodPassthrough) {
  auto proxy = CreateProxy();

  // Garbage pointers are used because the proxy just passes them along and does
  // absolutely nothing with them.
  auto* texture = CreateGarbagePtr<ID3D11Texture2D>();
  auto* out_desc = CreateGarbagePtr<D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC>();
  auto* in_desc = CreateGarbagePtr<D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC>();
  auto* out_view = CreateGarbagePtr<ID3D11VideoProcessorOutputView>();
  auto* streams = CreateGarbagePtr<D3D11_VIDEO_PROCESSOR_STREAM>();

  EXPECT_CALL(*dev_.Get(), CreateVideoProcessorOutputView(
                               texture, enumerator_.Get(), out_desc, nullptr));

  EXPECT_CALL(*dev_.Get(), CreateVideoProcessorInputView(
                               texture, enumerator_.Get(), in_desc, nullptr));

  EXPECT_CALL(*vctx_.Get(),
              VideoProcessorBlt(proc_.Get(), out_view, 6, 7, streams));

  EXPECT_TRUE(proxy->Init(0, 0).is_ok());
  proxy->CreateVideoProcessorOutputView(texture, out_desc, nullptr);
  proxy->CreateVideoProcessorInputView(texture, in_desc, nullptr);
  proxy->VideoProcessorBlt(out_view, 6, 7, streams);
}

}  // namespace media
