// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_service.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/stable_video_decoder_factory_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

namespace media {

namespace {

class MockVideoDecoder : public mojom::VideoDecoder {
 public:
  MockVideoDecoder() = default;
  MockVideoDecoder(const MockVideoDecoder&) = delete;
  MockVideoDecoder& operator=(const MockVideoDecoder&) = delete;
  ~MockVideoDecoder() override = default;

  // mojom::VideoDecoder implementation.
  MOCK_METHOD1(GetSupportedConfigs, void(GetSupportedConfigsCallback callback));
  MOCK_METHOD6(
      Construct,
      void(mojo::PendingAssociatedRemote<mojom::VideoDecoderClient> client,
           mojo::PendingRemote<mojom::MediaLog> media_log,
           mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
               video_frame_handle_receiver,
           mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
           mojom::CommandBufferIdPtr command_buffer_id,
           const gfx::ColorSpace& target_color_space));
  MOCK_METHOD4(Initialize,
               void(const VideoDecoderConfig& config,
                    bool low_delay,
                    const absl::optional<base::UnguessableToken>& cdm_id,
                    InitializeCallback callback));
  MOCK_METHOD2(Decode,
               void(mojom::DecoderBufferPtr buffer, DecodeCallback callback));
  MOCK_METHOD1(Reset, void(ResetCallback callback));
  MOCK_METHOD1(OnOverlayInfoChanged, void(const OverlayInfo& overlay_info));
};

class StableVideoDecoderServiceTest : public testing::Test {
 public:
  StableVideoDecoderServiceTest() {
    stable_video_decoder_factory_service_
        .SetVideoDecoderCreationCallbackForTesting(
            video_decoder_creation_cb_.Get());
  }

  StableVideoDecoderServiceTest(const StableVideoDecoderServiceTest&) = delete;
  StableVideoDecoderServiceTest& operator=(
      const StableVideoDecoderServiceTest&) = delete;
  ~StableVideoDecoderServiceTest() override = default;

  void SetUp() override {
    mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory>
        stable_video_decoder_factory_receiver;
    stable_video_decoder_factory_remote_ =
        mojo::Remote<stable::mojom::StableVideoDecoderFactory>(
            stable_video_decoder_factory_receiver
                .InitWithNewPipeAndPassRemote());
    stable_video_decoder_factory_service_.BindReceiver(
        std::move(stable_video_decoder_factory_receiver));
    ASSERT_TRUE(stable_video_decoder_factory_remote_.is_connected());
  }

 protected:
  mojo::Remote<stable::mojom::StableVideoDecoder> CreateStableVideoDecoder(
      std::unique_ptr<StrictMock<MockVideoDecoder>> dst_video_decoder) {
    // Each CreateStableVideoDecoder() should result in exactly one call to the
    // video decoder creation callback, i.e., the
    // StableVideoDecoderFactoryService should not re-use mojom::VideoDecoder
    // implementation instances.
    EXPECT_CALL(video_decoder_creation_cb_, Run(_, _))
        .WillOnce(Return(ByMove(std::move(dst_video_decoder))));
    mojo::PendingReceiver<stable::mojom::StableVideoDecoder>
        stable_video_decoder_receiver;
    mojo::Remote<stable::mojom::StableVideoDecoder> video_decoder_remote(
        stable_video_decoder_receiver.InitWithNewPipeAndPassRemote());
    stable_video_decoder_factory_remote_->CreateStableVideoDecoder(
        std::move(stable_video_decoder_receiver));
    stable_video_decoder_factory_remote_.FlushForTesting();
    if (!Mock::VerifyAndClearExpectations(&video_decoder_creation_cb_))
      return {};
    return video_decoder_remote;
  }

  base::test::TaskEnvironment task_environment_;
  StrictMock<base::MockRepeatingCallback<std::unique_ptr<
      mojom::VideoDecoder>(MojoMediaClient*, MojoCdmServiceContext*)>>
      video_decoder_creation_cb_;
  StableVideoDecoderFactoryService stable_video_decoder_factory_service_;
  mojo::Remote<stable::mojom::StableVideoDecoderFactory>
      stable_video_decoder_factory_remote_;
  mojo::Remote<stable::mojom::StableVideoDecoder> stable_video_decoder_remote_;
};

// Tests that we can create multiple StableVideoDecoder implementation instances
// through the StableVideoDecoderFactory and that they can exist concurrently.
TEST_F(StableVideoDecoderServiceTest, FactoryCanCreateStableVideoDecoders) {
  std::vector<mojo::Remote<stable::mojom::StableVideoDecoder>>
      stable_video_decoder_remotes;
  constexpr size_t kNumConcurrentDecoders = 5u;
  for (size_t i = 0u; i < kNumConcurrentDecoders; i++) {
    auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
    auto stable_video_decoder_remote =
        CreateStableVideoDecoder(std::move(mock_video_decoder));
    stable_video_decoder_remotes.push_back(
        std::move(stable_video_decoder_remote));
  }
  for (const auto& remote : stable_video_decoder_remotes) {
    ASSERT_TRUE(remote.is_bound());
    ASSERT_TRUE(remote.is_connected());
  }
}

}  // namespace

}  // namespace media
