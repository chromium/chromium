// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mman.h>

#include "base/posix/eintr_wrapper.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/stable_video_decoder_factory_service.h"
#include "media/mojo/services/stable_video_decoder_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_memory_buffer.h"

using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArgs;

namespace media {

namespace {

VideoDecoderConfig CreateValidVideoDecoderConfig() {
  const VideoDecoderConfig config(
      VideoCodec::kH264, VideoCodecProfile::H264PROFILE_BASELINE,
      VideoDecoderConfig::AlphaMode::kHasAlpha, VideoColorSpace::REC709(),
      VideoTransformation(VIDEO_ROTATION_90, /*mirrored=*/true),
      /*coded_size=*/gfx::Size(640, 368),
      /*visible_rect=*/gfx::Rect(1, 1, 630, 360),
      /*natural_size=*/gfx::Size(1260, 720),
      /*extra_data=*/std::vector<uint8_t>{1, 2, 3},
      EncryptionScheme::kUnencrypted);
  DCHECK(config.IsValidConfig());
  return config;
}

scoped_refptr<VideoFrame> CreateTestNV12GpuMemoryBufferVideoFrame() {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;

  // We need to create something that looks like a dma-buf in order to pass the
  // validation in the mojo traits, so we use memfd_create() + ftruncate().
  auto y_fd = base::ScopedFD(memfd_create("nv12_dummy_buffer", 0));
  if (!y_fd.is_valid()) {
    return nullptr;
  }
  if (HANDLE_EINTR(ftruncate(y_fd.get(), 280000 + 140000)) < 0) {
    return nullptr;
  }
  auto uv_fd = base::ScopedFD(HANDLE_EINTR(dup(y_fd.get())));
  if (!uv_fd.is_valid()) {
    return nullptr;
  }

  gfx::NativePixmapPlane y_plane;
  y_plane.stride = 700;
  y_plane.offset = 0;
  y_plane.size = 280000;
  y_plane.fd = std::move(y_fd);
  gmb_handle.native_pixmap_handle.planes.push_back(std::move(y_plane));

  gfx::NativePixmapPlane uv_plane;
  uv_plane.stride = 700;
  uv_plane.offset = 280000;
  uv_plane.size = 140000;
  uv_plane.fd = std::move(uv_fd);
  gmb_handle.native_pixmap_handle.planes.push_back(std::move(uv_plane));

  gpu::GpuMemoryBufferSupport gmb_support;
  auto gmb = gmb_support.CreateGpuMemoryBufferImplFromHandle(
      std::move(gmb_handle), gfx::Size(640, 368),
      gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferUsage::SCANOUT_VDA_WRITE,
      base::DoNothing());
  if (!gmb) {
    return nullptr;
  }

  auto gmb_video_frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      /*visible_rect=*/gfx::Rect(640, 368),
      /*natural_size=*/gfx::Size(640, 368), std::move(gmb), base::TimeDelta());
  if (!gmb_video_frame) {
    return nullptr;
  }

  gmb_video_frame->metadata().allow_overlay = true;
  gmb_video_frame->metadata().end_of_stream = false;
  gmb_video_frame->metadata().read_lock_fences_enabled = true;
  gmb_video_frame->metadata().power_efficient = true;

  return gmb_video_frame;
}

class MockVideoFrameHandleReleaser : public mojom::VideoFrameHandleReleaser {
 public:
  explicit MockVideoFrameHandleReleaser(
      mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
          video_frame_handle_releaser)
      : video_frame_handle_releaser_receiver_(
            this,
            std::move(video_frame_handle_releaser)) {}
  MockVideoFrameHandleReleaser(const MockVideoFrameHandleReleaser&) = delete;
  MockVideoFrameHandleReleaser& operator=(const MockVideoFrameHandleReleaser&) =
      delete;
  ~MockVideoFrameHandleReleaser() override = default;

  // mojom::VideoFrameHandleReleaser implementation.
  MOCK_METHOD2(ReleaseVideoFrame,
               void(const base::UnguessableToken& release_token,
                    const std::optional<gpu::SyncToken>& release_sync_token));

 private:
  mojo::Receiver<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_receiver_;
};

class MockVideoDecoder : public mojom::VideoDecoder {
 public:
  MockVideoDecoder() = default;
  MockVideoDecoder(const MockVideoDecoder&) = delete;
  MockVideoDecoder& operator=(const MockVideoDecoder&) = delete;
  ~MockVideoDecoder() override = default;

  mojo::AssociatedRemote<mojom::VideoDecoderClient> TakeClientRemote() {
    return std::move(client_remote_);
  }
  mojo::Remote<mojom::MediaLog> TakeMediaLogRemote() {
    return std::move(media_log_remote_);
  }
  std::unique_ptr<StrictMock<MockVideoFrameHandleReleaser>>
  TakeVideoFrameHandleReleaser() {
    return std::move(video_frame_handle_releaser_);
  }
  std::unique_ptr<MojoDecoderBufferReader> TakeMojoDecoderBufferReader() {
    return std::move(mojo_decoder_buffer_reader_);
  }

  // mojom::VideoDecoder implementation.
  MOCK_METHOD1(GetSupportedConfigs, void(GetSupportedConfigsCallback callback));
  void Construct(
      mojo::PendingAssociatedRemote<mojom::VideoDecoderClient> client,
      mojo::PendingRemote<mojom::MediaLog> media_log,
      mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
          video_frame_handle_releaser,
      mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
      mojom::CommandBufferIdPtr command_buffer_id,
      const gfx::ColorSpace& target_color_space) final {
    client_remote_.Bind(std::move(client));
    media_log_remote_.Bind(std::move(media_log));
    video_frame_handle_releaser_ =
        std::make_unique<StrictMock<MockVideoFrameHandleReleaser>>(
            std::move(video_frame_handle_releaser));
    mojo_decoder_buffer_reader_ = std::make_unique<MojoDecoderBufferReader>(
        std::move(decoder_buffer_pipe));
    DoConstruct(std::move(command_buffer_id), target_color_space);
  }
  MOCK_METHOD2(DoConstruct,
               void(mojom::CommandBufferIdPtr command_buffer_id,
                    const gfx::ColorSpace& target_color_space));
  MOCK_METHOD4(Initialize,
               void(const VideoDecoderConfig& config,
                    bool low_delay,
                    const std::optional<base::UnguessableToken>& cdm_id,
                    InitializeCallback callback));
  MOCK_METHOD2(Decode,
               void(mojom::DecoderBufferPtr buffer, DecodeCallback callback));
  MOCK_METHOD1(Reset, void(ResetCallback callback));
  MOCK_METHOD1(OnOverlayInfoChanged, void(const OverlayInfo& overlay_info));

 private:
  mojo::AssociatedRemote<mojom::VideoDecoderClient> client_remote_;
  mojo::Remote<mojom::MediaLog> media_log_remote_;
  std::unique_ptr<StrictMock<MockVideoFrameHandleReleaser>>
      video_frame_handle_releaser_;
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;
};

class MockStableVideoDecoderTracker
    : public stable::mojom::StableVideoDecoderTracker {};

class MockStableVideoDecoderClient : public stable::mojom::VideoDecoderClient {
 public:
  explicit MockStableVideoDecoderClient(
      mojo::PendingAssociatedReceiver<stable::mojom::VideoDecoderClient>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  MockStableVideoDecoderClient(const MockStableVideoDecoderClient&) = delete;
  MockStableVideoDecoderClient& operator=(const MockStableVideoDecoderClient&) =
      delete;
  ~MockStableVideoDecoderClient() override = default;

  // stable::mojom::VideoDecoderClient implementation.
  MOCK_METHOD3(OnVideoFrameDecoded,
               void(stable::mojom::VideoFramePtr frame,
                    bool can_read_without_stalling,
                    const base::UnguessableToken& release_token));
  MOCK_METHOD1(OnWaiting, void(WaitingReason reason));

 private:
  mojo::AssociatedReceiver<stable::mojom::VideoDecoderClient> receiver_;
};

class MockStableMediaLog : public stable::mojom::MediaLog {
 public:
  explicit MockStableMediaLog(
      mojo::PendingReceiver<stable::mojom::MediaLog> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  MockStableMediaLog(const MockStableMediaLog&) = delete;
  MockStableMediaLog& operator=(const MockStableMediaLog&) = delete;
  ~MockStableMediaLog() override = default;

  // stable::mojom::MediaLog implementation.
  MOCK_METHOD1(AddLogRecord, void(const MediaLogRecord& event));

 private:
  mojo::Receiver<stable::mojom::MediaLog> receiver_;
};

// AuxiliaryEndpoints groups the endpoints that support the operation of a
// StableVideoDecoderService and that come from the Construct() call. That way,
// tests can easily poke at one endpoint and set expectations on the other. For
// example, a test might want to simulate the scenario in which a frame has been
// decoded by the underlying mojom::VideoDecoder. In this case, the test can
// call |video_decoder_client_remote|->OnVideoFrameDecoded() and then set an
// expectation on |mock_stable_video_decoder_client|->OnVideoFrameDecoded().
struct AuxiliaryEndpoints {
  // |video_decoder_client_remote| is the client that the underlying
  // mojom::VideoDecoder receives through the Construct() call. Tests can make
  // calls on it and those calls should ultimately be received by the
  // |mock_stable_video_decoder_client|.
  mojo::AssociatedRemote<mojom::VideoDecoderClient> video_decoder_client_remote;
  std::unique_ptr<StrictMock<MockStableVideoDecoderClient>>
      mock_stable_video_decoder_client;

  // |media_log_remote| is the MediaLog that the underlying mojom::VideoDecoder
  // receives through the Construct() call. Tests can make calls on it and those
  // calls should ultimately be received by the |mock_stable_media_log|.
  mojo::Remote<mojom::MediaLog> media_log_remote;
  std::unique_ptr<StrictMock<MockStableMediaLog>> mock_stable_media_log;

  // Tests can use |stable_video_frame_handle_releaser_remote| to simulate
  // releasing a VideoFrame.
  // |mock_video_frame_handle_releaser| is the VideoFrameHandleReleaser that's
  // setup when the underlying mojom::VideoDecoder receives a Construct() call.
  // Tests can make calls on |stable_video_frame_handle_releaser_remote| and
  // they should be ultimately received by the
  // |mock_video_frame_handle_releaser|.
  mojo::Remote<stable::mojom::VideoFrameHandleReleaser>
      stable_video_frame_handle_releaser_remote;
  std::unique_ptr<StrictMock<MockVideoFrameHandleReleaser>>
      mock_video_frame_handle_releaser;

  // |mojo_decoder_buffer_reader| wraps the reading end of the data pipe that
  // the underlying mojom::VideoDecoder receives through the Construct() call.
  // Tests can write data using the |mojo_decoder_buffer_writer| and that data
  // should be ultimately received by the |mojo_decoder_buffer_reader|.
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer;
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader;
};

// Calls Construct() on |stable_video_decoder_remote| and, if
// |expect_construct_call| is true, expects a corresponding Construct() call on
// |mock_video_decoder| which is assumed to be the backing decoder of
// |stable_video_decoder_remote|. Returns nullptr if the expectations on
// |mock_video_decoder| are violated. Otherwise, returns an AuxiliaryEndpoints
// instance that contains the supporting endpoints that tests can use to
// interact with the auxiliary interfaces used by the
// |stable_video_decoder_remote|.
std::unique_ptr<AuxiliaryEndpoints> ConstructStableVideoDecoder(
    mojo::Remote<stable::mojom::StableVideoDecoder>&
        stable_video_decoder_remote,
    StrictMock<MockVideoDecoder>& mock_video_decoder,
    bool expect_construct_call) {
  constexpr gfx::ColorSpace kTargetColorSpace = gfx::ColorSpace::CreateSRGB();
  if (expect_construct_call) {
    EXPECT_CALL(mock_video_decoder,
                DoConstruct(/*command_buffer_id=*/_,
                            /*target_color_space=*/kTargetColorSpace));
  }
  mojo::PendingAssociatedRemote<stable::mojom::VideoDecoderClient>
      stable_video_decoder_client_remote;
  auto mock_stable_video_decoder_client =
      std::make_unique<StrictMock<MockStableVideoDecoderClient>>(
          stable_video_decoder_client_remote
              .InitWithNewEndpointAndPassReceiver());

  mojo::PendingRemote<stable::mojom::MediaLog> stable_media_log_remote;
  auto mock_stable_media_log = std::make_unique<StrictMock<MockStableMediaLog>>(
      stable_media_log_remote.InitWithNewPipeAndPassReceiver());

  mojo::Remote<stable::mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_remote;

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer =
      MojoDecoderBufferWriter::Create(
          GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO),
          &remote_consumer_handle);

  stable_video_decoder_remote->Construct(
      std::move(stable_video_decoder_client_remote),
      std::move(stable_media_log_remote),
      video_frame_handle_releaser_remote.BindNewPipeAndPassReceiver(),
      std::move(remote_consumer_handle), kTargetColorSpace);
  stable_video_decoder_remote.FlushForTesting();

  if (!Mock::VerifyAndClearExpectations(&mock_video_decoder))
    return nullptr;

  auto auxiliary_endpoints = std::make_unique<AuxiliaryEndpoints>();

  auxiliary_endpoints->video_decoder_client_remote =
      mock_video_decoder.TakeClientRemote();
  auxiliary_endpoints->mock_stable_video_decoder_client =
      std::move(mock_stable_video_decoder_client);

  auxiliary_endpoints->media_log_remote =
      mock_video_decoder.TakeMediaLogRemote();
  auxiliary_endpoints->mock_stable_media_log = std::move(mock_stable_media_log);

  auxiliary_endpoints->stable_video_frame_handle_releaser_remote =
      std::move(video_frame_handle_releaser_remote);
  auxiliary_endpoints->mock_video_frame_handle_releaser =
      mock_video_decoder.TakeVideoFrameHandleReleaser();

  auxiliary_endpoints->mojo_decoder_buffer_writer =
      std::move(mojo_decoder_buffer_writer);
  auxiliary_endpoints->mojo_decoder_buffer_reader =
      mock_video_decoder.TakeMojoDecoderBufferReader();

  return auxiliary_endpoints;
}

class StableVideoDecoderServiceTest : public testing::Test {
 public:
  StableVideoDecoderServiceTest()
      : stable_video_decoder_factory_service_(gpu::GpuFeatureInfo()) {
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
        std::move(stable_video_decoder_factory_receiver),
        /*disconnect_cb=*/base::DoNothing());
    ASSERT_TRUE(stable_video_decoder_factory_remote_.is_connected());
  }

 protected:
  mojo::Remote<stable::mojom::StableVideoDecoder> CreateStableVideoDecoder(
      std::unique_ptr<StrictMock<MockVideoDecoder>> dst_video_decoder,
      mojo::PendingRemote<stable::mojom::StableVideoDecoderTracker> tracker) {
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
        std::move(stable_video_decoder_receiver), std::move(tracker));
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
        CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
    stable_video_decoder_remotes.push_back(
        std::move(stable_video_decoder_remote));
  }
  for (const auto& remote : stable_video_decoder_remotes) {
    ASSERT_TRUE(remote.is_bound());
    ASSERT_TRUE(remote.is_connected());
  }
}

// Tests that a call to stable::mojom::VideoDecoder::Construct() gets routed
// correctly to the underlying mojom::VideoDecoder.
TEST_F(StableVideoDecoderServiceTest, StableVideoDecoderCanBeConstructed) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  ASSERT_TRUE(ConstructStableVideoDecoder(stable_video_decoder_remote,
                                          *mock_video_decoder_raw,
                                          /*expect_construct_call=*/true));
}

// Tests that if two calls to stable::mojom::VideoDecoder::Construct() are made,
// only one is routed to the underlying mojom::VideoDecoder.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderCannotBeConstructedTwice) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  EXPECT_TRUE(ConstructStableVideoDecoder(stable_video_decoder_remote,
                                          *mock_video_decoder_raw,
                                          /*expect_construct_call=*/true));
  EXPECT_TRUE(ConstructStableVideoDecoder(stable_video_decoder_remote,
                                          *mock_video_decoder_raw,
                                          /*expect_construct_call=*/false));
}

// Tests that a call to stable::mojom::VideoDecoder::GetSupportedConfigs() gets
// routed correctly to the underlying mojom::VideoDecoder. Also tests that the
// underlying mojom::VideoDecoder's reply gets routed correctly back to the
// client.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderCanGetSupportedConfigs) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());

  StrictMock<base::MockOnceCallback<void(
      const std::vector<SupportedVideoDecoderConfig>& supported_configs,
      VideoDecoderType decoder_type)>>
      get_supported_configs_cb_to_send;
  mojom::VideoDecoder::GetSupportedConfigsCallback
      received_get_supported_configs_cb;
  constexpr VideoDecoderType kDecoderTypeToReplyWith = VideoDecoderType::kVaapi;
  const std::vector<SupportedVideoDecoderConfig>
      supported_configs_to_reply_with({
          {/*profile_min=*/H264PROFILE_MIN, /*profile_max=*/H264PROFILE_MAX,
           /*coded_size_min=*/gfx::Size(320, 180),
           /*coded_size_max=*/gfx::Size(1280, 720), /*allow_encrypted=*/false,
           /*require_encrypted=*/false},
          {/*profile_min=*/VP9PROFILE_MIN, /*profile_max=*/VP9PROFILE_MAX,
           /*coded_size_min=*/gfx::Size(8, 8),
           /*coded_size_max=*/gfx::Size(640, 360), /*allow_encrypted=*/true,
           /*require_encrypted=*/true},
      });
  std::vector<SupportedVideoDecoderConfig> received_supported_configs;

  EXPECT_CALL(*mock_video_decoder_raw, GetSupportedConfigs(/*callback=*/_))
      .WillOnce([&](mojom::VideoDecoder::GetSupportedConfigsCallback callback) {
        received_get_supported_configs_cb = std::move(callback);
      });
  EXPECT_CALL(get_supported_configs_cb_to_send, Run(_, kDecoderTypeToReplyWith))
      .WillOnce(SaveArg<0>(&received_supported_configs));

  stable_video_decoder_remote->GetSupportedConfigs(
      get_supported_configs_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(mock_video_decoder_raw));

  std::move(received_get_supported_configs_cb)
      .Run(supported_configs_to_reply_with, kDecoderTypeToReplyWith);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(received_supported_configs, supported_configs_to_reply_with);
}

// Tests that a call to stable::mojom::VideoDecoder::Initialize() gets routed
// correctly to the underlying mojom::VideoDecoder. Also tests that when the
// underlying mojom::VideoDecoder calls the initialization callback, the call
// gets routed to the client.
TEST_F(StableVideoDecoderServiceTest, StableVideoDecoderCanBeInitialized) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);

  const VideoDecoderConfig config_to_send = CreateValidVideoDecoderConfig();
  VideoDecoderConfig received_config;
  constexpr bool kLowDelay = true;
  constexpr std::optional<base::UnguessableToken> kCdmId = std::nullopt;
  StrictMock<base::MockOnceCallback<void(
      const media::DecoderStatus& status, bool needs_bitstream_conversion,
      int32_t max_decode_requests, VideoDecoderType decoder_type,
      bool needs_transcryption)>>
      initialize_cb_to_send;
  mojom::VideoDecoder::InitializeCallback received_initialize_cb;
  const DecoderStatus kDecoderStatus = DecoderStatus::Codes::kAborted;
  constexpr bool kNeedsBitstreamConversion = true;
  constexpr int32_t kMaxDecodeRequests = 123;
  constexpr VideoDecoderType kDecoderType = VideoDecoderType::kVda;

  EXPECT_CALL(*mock_video_decoder_raw,
              Initialize(/*config=*/_, kLowDelay, kCdmId,
                         /*callback=*/_))
      .WillOnce([&](const VideoDecoderConfig& config, bool low_delay,
                    const std::optional<base::UnguessableToken>& cdm_id,
                    mojom::VideoDecoder::InitializeCallback callback) {
        received_config = config;
        received_initialize_cb = std::move(callback);
      });
  EXPECT_CALL(initialize_cb_to_send,
              Run(kDecoderStatus, kNeedsBitstreamConversion, kMaxDecodeRequests,
                  kDecoderType, /*needs_transcryption=*/false));
  stable_video_decoder_remote->Initialize(
      config_to_send, kLowDelay,
      mojo::PendingRemote<stable::mojom::StableCdmContext>(),
      initialize_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(mock_video_decoder_raw));

  std::move(received_initialize_cb)
      .Run(kDecoderStatus, kNeedsBitstreamConversion, kMaxDecodeRequests,
           kDecoderType);
  task_environment_.RunUntilIdle();
}

// Tests that the StableVideoDecoderService rejects a call to
// stable::mojom::VideoDecoder::Initialize() before
// stable::mojom::VideoDecoder::Construct() gets called.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderCannotBeInitializedBeforeConstruction) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());

  const VideoDecoderConfig config_to_send = CreateValidVideoDecoderConfig();
  constexpr bool kLowDelay = true;
  StrictMock<base::MockOnceCallback<void(
      const media::DecoderStatus& status, bool needs_bitstream_conversion,
      int32_t max_decode_requests, VideoDecoderType decoder_type,
      bool needs_transcryption)>>
      initialize_cb_to_send;

  EXPECT_CALL(initialize_cb_to_send,
              Run(DecoderStatus(DecoderStatus::Codes::kFailed),
                  /*needs_bitstream_conversion=*/false,
                  /*max_decode_requests=*/1, VideoDecoderType::kUnknown,
                  /*needs_transcryption=*/false));
  stable_video_decoder_remote->Initialize(
      config_to_send, kLowDelay,
      mojo::PendingRemote<stable::mojom::StableCdmContext>(),
      initialize_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
}

// Tests that a call to stable::mojom::VideoDecoder::Decode() gets routed
// correctly to the underlying mojom::VideoDecoder and that the data pipe is
// plumbed correctly. Also tests that when the underlying mojom::VideoDecoder
// calls the decode callback, the call gets routed to the client.
TEST_F(StableVideoDecoderServiceTest, StableVideoDecoderCanDecode) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->mojo_decoder_buffer_writer);
  ASSERT_TRUE(auxiliary_endpoints->mojo_decoder_buffer_reader);

  constexpr uint8_t kEncodedData[] = {1, 2, 3};
  scoped_refptr<DecoderBuffer> decoder_buffer_to_send =
      DecoderBuffer::CopyFrom(kEncodedData);
  decoder_buffer_to_send->WritableSideData().secure_handle = 42;
  ASSERT_TRUE(decoder_buffer_to_send);
  mojom::DecoderBufferPtr received_decoder_buffer_ptr;
  scoped_refptr<DecoderBuffer> received_decoder_buffer;
  StrictMock<base::MockOnceCallback<void(const media::DecoderStatus& status)>>
      decode_cb_to_send;
  mojom::VideoDecoder::DecodeCallback received_decode_cb;
  const DecoderStatus kDecoderStatus = DecoderStatus::Codes::kAborted;

  EXPECT_CALL(*mock_video_decoder_raw, Decode(/*buffer=*/_, /*callback=*/_))
      .WillOnce([&](mojom::DecoderBufferPtr buffer,
                    mojom::VideoDecoder::DecodeCallback callback) {
        received_decoder_buffer_ptr = std::move(buffer);
        received_decode_cb = std::move(callback);
      });
  EXPECT_CALL(decode_cb_to_send, Run(kDecoderStatus));
  ASSERT_TRUE(
      auxiliary_endpoints->mojo_decoder_buffer_writer->WriteDecoderBuffer(
          decoder_buffer_to_send));
  stable_video_decoder_remote->Decode(decoder_buffer_to_send,
                                      decode_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(mock_video_decoder_raw));

  ASSERT_TRUE(received_decoder_buffer_ptr);
  auxiliary_endpoints->mojo_decoder_buffer_reader->ReadDecoderBuffer(
      std::move(received_decoder_buffer_ptr),
      base::BindOnce(
          [](scoped_refptr<DecoderBuffer>* dst_buffer,
             scoped_refptr<DecoderBuffer> buffer) {
            *dst_buffer = std::move(buffer);
          },
          &received_decoder_buffer));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_decoder_buffer);
  EXPECT_TRUE(
      received_decoder_buffer->MatchesForTesting(*decoder_buffer_to_send));

  std::move(received_decode_cb).Run(kDecoderStatus);
  task_environment_.RunUntilIdle();
}

// Tests that the StableVideoDecoderService rejects a call to
// stable::mojom::VideoDecoder::Decode() before
// stable::mojom::VideoDecoder::Construct() gets called.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderCannotDecodeBeforeConstruction) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());

  constexpr uint8_t kEncodedData[] = {1, 2, 3};
  scoped_refptr<DecoderBuffer> decoder_buffer_to_send =
      DecoderBuffer::CopyFrom(kEncodedData);
  ASSERT_TRUE(decoder_buffer_to_send);
  StrictMock<base::MockOnceCallback<void(const media::DecoderStatus& status)>>
      decode_cb_to_send;

  EXPECT_CALL(decode_cb_to_send,
              Run(DecoderStatus(DecoderStatus::Codes::kFailed)));
  stable_video_decoder_remote->Decode(decoder_buffer_to_send,
                                      decode_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
}

// Tests that a call to stable::mojom::VideoDecoder::Reset() gets routed
// correctly to the underlying mojom::VideoDecoder. Also tests that when the
// underlying mojom::VideoDecoder calls the reset callback, the call gets routed
// to the client.
TEST_F(StableVideoDecoderServiceTest, StableVideoDecoderCanBeReset) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);

  StrictMock<base::MockOnceCallback<void()>> reset_cb_to_send;
  mojom::VideoDecoder::ResetCallback received_reset_cb;

  EXPECT_CALL(*mock_video_decoder_raw, Reset(/*callback=*/_))
      .WillOnce([&](mojom::VideoDecoder::ResetCallback callback) {
        received_reset_cb = std::move(callback);
      });
  EXPECT_CALL(reset_cb_to_send, Run());
  stable_video_decoder_remote->Reset(reset_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(mock_video_decoder_raw));

  std::move(received_reset_cb).Run();
  task_environment_.RunUntilIdle();
}

// Tests that the StableVideoDecoderService doesn't route a
// stable::mojom::VideoDecoder::Reset() call to the underlying
// mojom::VideoDecoder before stable::mojom::VideoDecoder::Construct() gets
// called and that it just calls the reset callback.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderCannotBeResetBeforeConstruction) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());

  StrictMock<base::MockOnceCallback<void()>> reset_cb_to_send;

  EXPECT_CALL(reset_cb_to_send, Run());
  stable_video_decoder_remote->Reset(reset_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
}

// Tests that a call to
// stable::mojom::VideoFrameHandleReleaser::ReleaseVideoFrame() gets routed
// correctly to the underlying mojom::VideoFrameHandleReleaser.
TEST_F(StableVideoDecoderServiceTest, VideoFramesCanBeReleased) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->stable_video_frame_handle_releaser_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_video_frame_handle_releaser);

  const base::UnguessableToken release_token_to_send =
      base::UnguessableToken::Create();
  const std::optional<gpu::SyncToken> expected_release_sync_token =
      std::nullopt;

  EXPECT_CALL(
      *auxiliary_endpoints->mock_video_frame_handle_releaser,
      ReleaseVideoFrame(release_token_to_send, expected_release_sync_token));
  auxiliary_endpoints->stable_video_frame_handle_releaser_remote
      ->ReleaseVideoFrame(release_token_to_send);
  auxiliary_endpoints->stable_video_frame_handle_releaser_remote
      .FlushForTesting();
}

TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderClientReceivesOnVideoFrameDecodedEvent) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->video_decoder_client_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_stable_video_decoder_client);

  const auto token_for_release = base::UnguessableToken::Create();
  scoped_refptr<VideoFrame> video_frame_to_send =
      CreateTestNV12GpuMemoryBufferVideoFrame();
  ASSERT_TRUE(video_frame_to_send);
  stable::mojom::VideoFramePtr video_frame_received;
  constexpr bool kCanReadWithoutStalling = true;
  EXPECT_CALL(
      *auxiliary_endpoints->mock_stable_video_decoder_client,
      OnVideoFrameDecoded(_, kCanReadWithoutStalling, token_for_release))
      .WillOnce(WithArgs<0>(
          [&video_frame_received](stable::mojom::VideoFramePtr frame) {
            video_frame_received = std::move(frame);
          }));
  auxiliary_endpoints->video_decoder_client_remote->OnVideoFrameDecoded(
      video_frame_to_send, kCanReadWithoutStalling, token_for_release);
  auxiliary_endpoints->video_decoder_client_remote.FlushForTesting();
  ASSERT_TRUE(video_frame_received);
  EXPECT_FALSE(video_frame_received->metadata.end_of_stream);
  EXPECT_TRUE(video_frame_received->metadata.read_lock_fences_enabled);
  EXPECT_TRUE(video_frame_received->metadata.power_efficient);
  EXPECT_TRUE(video_frame_received->metadata.allow_overlay);
}

// Tests that a mojom::VideoDecoderClient::OnWaiting() call originating from the
// underlying mojom::VideoDecoder gets forwarded to the
// stable::mojom::VideoDecoderClient correctly.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderClientReceivesOnWaitingEvent) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->video_decoder_client_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_stable_video_decoder_client);

  constexpr WaitingReason kWaitingReason = WaitingReason::kNoDecryptionKey;
  EXPECT_CALL(*auxiliary_endpoints->mock_stable_video_decoder_client,
              OnWaiting(kWaitingReason));
  auxiliary_endpoints->video_decoder_client_remote->OnWaiting(kWaitingReason);
  auxiliary_endpoints->video_decoder_client_remote.FlushForTesting();
}

// Tests that a mojom::MediaLog::AddLogRecord() call originating from the
// underlying mojom::VideoDecoder gets forwarded to the stable::mojom::MediaLog
// correctly.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderClientReceivesAddLogRecordEvent) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->media_log_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_stable_media_log);

  MediaLogRecord media_log_record_to_send;
  media_log_record_to_send.id = 2;
  media_log_record_to_send.type = MediaLogRecord::Type::kMediaStatus;
  media_log_record_to_send.params.Set("Test", "Value");
  media_log_record_to_send.time = base::TimeTicks::Now();

  EXPECT_CALL(*auxiliary_endpoints->mock_stable_media_log,
              AddLogRecord(media_log_record_to_send));
  auxiliary_endpoints->media_log_remote->AddLogRecord(media_log_record_to_send);
  auxiliary_endpoints->media_log_remote.FlushForTesting();
}

// Tests that a StableVideoDecoderTracker can be used to know when the remote
// StableVideoDecoder implementation dies.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderTrackerDisconnectsWhenStableVideoDecoderDies) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();

  MockStableVideoDecoderTracker tracker;
  mojo::Receiver<stable::mojom::StableVideoDecoderTracker> tracker_receiver(
      &tracker);
  mojo::PendingRemote<stable::mojom::StableVideoDecoderTracker> tracker_remote =
      tracker_receiver.BindNewPipeAndPassRemote();
  StrictMock<base::MockOnceCallback<void()>> tracker_disconnect_cb;
  tracker_receiver.set_disconnect_handler(tracker_disconnect_cb.Get());

  auto stable_video_decoder_remote = CreateStableVideoDecoder(
      std::move(mock_video_decoder), std::move(tracker_remote));
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());

  // Until now, nothing in particular should happen.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(&tracker_disconnect_cb));

  // Once we reset the |stable_video_decoder_remote|, the StableVideoDecoder
  // implementation should die and the |tracker| should get disconnected.
  EXPECT_CALL(tracker_disconnect_cb, Run());
  stable_video_decoder_remote.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace media
