// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/oop_video_decoder_service.h"

#include <sys/mman.h>

#include "base/posix/eintr_wrapper.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/oop_video_decoder_factory_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"

using testing::_;
using testing::ByMove;
using testing::IsTrue;
using testing::Mock;
using testing::Property;
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

scoped_refptr<VideoFrame> CreateTestNV12VideoFrame() {
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
  std::vector<base::ScopedFD> dmabuf_fds;
  dmabuf_fds.emplace_back(std::move(y_fd));
  dmabuf_fds.emplace_back(std::move(uv_fd));

  std::vector<ColorPlaneLayout> planes;
  ColorPlaneLayout y_plane;
  y_plane.stride = 700;
  y_plane.offset = 0;
  y_plane.size = 280000;
  planes.emplace_back(std::move(y_plane));

  ColorPlaneLayout uv_plane;
  uv_plane.stride = 700;
  uv_plane.offset = 280000;
  uv_plane.size = 140000;
  planes.emplace_back(std::move(uv_plane));

  std::optional<VideoFrameLayout> layout = VideoFrameLayout::CreateWithPlanes(
      /*format=*/PIXEL_FORMAT_NV12, /*coded_size=*/gfx::Size(640, 368),
      std::move(planes));
  if (!layout.has_value()) {
    return nullptr;
  }

  auto video_frame = VideoFrame::WrapExternalDmabufs(
      *layout, /*visible_rect=*/gfx::Rect(640, 368),
      /*natural_size=*/gfx::Size(640, 368), std::move(dmabuf_fds),
      base::TimeDelta());
  if (!video_frame) {
    return nullptr;
  }

  video_frame->metadata().allow_overlay = true;
  video_frame->metadata().end_of_stream = false;
  video_frame->metadata().read_lock_fences_enabled = true;
  video_frame->metadata().power_efficient = true;

  return video_frame;
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
                    mojom::CdmPtr cdm,
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

class MockVideoDecoderTracker : public mojom::VideoDecoderTracker {};

class MockVideoDecoderClient : public mojom::VideoDecoderClient {
 public:
  explicit MockVideoDecoderClient(
      mojo::PendingAssociatedReceiver<mojom::VideoDecoderClient>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  MockVideoDecoderClient(const MockVideoDecoderClient&) = delete;
  MockVideoDecoderClient& operator=(const MockVideoDecoderClient&) = delete;
  ~MockVideoDecoderClient() override = default;

  // mojom::VideoDecoderClient implementation.
  MOCK_METHOD3(
      OnVideoFrameDecoded,
      void(const scoped_refptr<VideoFrame>& frame,
           bool can_read_without_stalling,
           const std::optional<base::UnguessableToken>& release_token));
  MOCK_METHOD1(OnWaiting, void(WaitingReason reason));
  MOCK_METHOD0(RequestOverlayInfo, void());

 private:
  mojo::AssociatedReceiver<mojom::VideoDecoderClient> receiver_;
};

class MockMediaLog : public mojom::MediaLog {
 public:
  explicit MockMediaLog(mojo::PendingReceiver<mojom::MediaLog> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  MockMediaLog(const MockMediaLog&) = delete;
  MockMediaLog& operator=(const MockMediaLog&) = delete;
  ~MockMediaLog() override = default;

  // mojom::MediaLog implementation.
  MOCK_METHOD1(AddLogRecord, void(const MediaLogRecord& event));

 private:
  mojo::Receiver<mojom::MediaLog> receiver_;
};

// AuxiliaryEndpoints groups the endpoints that support the operation of a
// OOPVideoDecoderService and that come from the Construct() call. That way,
// tests can easily poke at one endpoint and set expectations on the other. For
// example, a test might want to simulate the scenario in which a frame has been
// decoded by the underlying mojom::VideoDecoder. In this case, the test can
// call |video_decoder_client_remote|->OnVideoFrameDecoded() and then set an
// expectation on |mock_video_decoder_client|->OnVideoFrameDecoded().
struct AuxiliaryEndpoints {
  // |video_decoder_client_remote| is the client that the underlying
  // mojom::VideoDecoder receives through the Construct() call. Tests can make
  // calls on it and those calls should ultimately be received by the
  // |mock_video_decoder_client|.
  mojo::AssociatedRemote<mojom::VideoDecoderClient> video_decoder_client_remote;
  std::unique_ptr<StrictMock<MockVideoDecoderClient>> mock_video_decoder_client;

  // |media_log_remote| is the MediaLog that the underlying mojom::VideoDecoder
  // receives through the Construct() call. Tests can make calls on it and those
  // calls should ultimately be received by the |mock_media_log|.
  mojo::Remote<mojom::MediaLog> media_log_remote;
  std::unique_ptr<StrictMock<MockMediaLog>> mock_media_log;

  // Tests can use |video_frame_handle_releaser_remote| to simulate
  // releasing a VideoFrame.
  // |mock_video_frame_handle_releaser| is the VideoFrameHandleReleaser that's
  // setup when the underlying mojom::VideoDecoder receives a Construct() call.
  // Tests can make calls on |video_frame_handle_releaser_remote| and
  // they should be ultimately received by the
  // |mock_video_frame_handle_releaser|.
  mojo::Remote<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_remote;
  std::unique_ptr<StrictMock<MockVideoFrameHandleReleaser>>
      mock_video_frame_handle_releaser;

  // |mojo_decoder_buffer_reader| wraps the reading end of the data pipe that
  // the underlying mojom::VideoDecoder receives through the Construct() call.
  // Tests can write data using the |mojo_decoder_buffer_writer| and that data
  // should be ultimately received by the |mojo_decoder_buffer_reader|.
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer;
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader;
};

// Calls Construct() on |video_decoder_remote| and, if
// |expect_construct_call| is true, expects a corresponding Construct() call on
// |mock_video_decoder| which is assumed to be the backing decoder of
// |video_decoder_remote|. Returns nullptr if the expectations on
// |mock_video_decoder| are violated. Otherwise, returns an AuxiliaryEndpoints
// instance that contains the supporting endpoints that tests can use to
// interact with the auxiliary interfaces used by the
// |video_decoder_remote|.
std::unique_ptr<AuxiliaryEndpoints> ConstructVideoDecoder(
    mojo::Remote<mojom::VideoDecoder>& video_decoder_remote,
    StrictMock<MockVideoDecoder>& mock_video_decoder,
    bool expect_construct_call) {
  constexpr gfx::ColorSpace kTargetColorSpace = gfx::ColorSpace::CreateSRGB();
  if (expect_construct_call) {
    EXPECT_CALL(mock_video_decoder,
                DoConstruct(/*command_buffer_id=*/_,
                            /*target_color_space=*/kTargetColorSpace));
  }
  mojo::PendingAssociatedRemote<mojom::VideoDecoderClient>
      video_decoder_client_remote;
  auto mock_video_decoder_client =
      std::make_unique<StrictMock<MockVideoDecoderClient>>(
          video_decoder_client_remote.InitWithNewEndpointAndPassReceiver());

  mojo::PendingRemote<mojom::MediaLog> media_log_remote;
  auto mock_media_log = std::make_unique<StrictMock<MockMediaLog>>(
      media_log_remote.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_remote;

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer =
      MojoDecoderBufferWriter::Create(
          GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO),
          &remote_consumer_handle);

  video_decoder_remote->Construct(
      std::move(video_decoder_client_remote), std::move(media_log_remote),
      video_frame_handle_releaser_remote.BindNewPipeAndPassReceiver(),
      std::move(remote_consumer_handle), media::mojom::CommandBufferIdPtr(),
      kTargetColorSpace);
  video_decoder_remote.FlushForTesting();

  if (!Mock::VerifyAndClearExpectations(&mock_video_decoder)) {
    return nullptr;
  }

  auto auxiliary_endpoints = std::make_unique<AuxiliaryEndpoints>();

  auxiliary_endpoints->video_decoder_client_remote =
      mock_video_decoder.TakeClientRemote();
  auxiliary_endpoints->mock_video_decoder_client =
      std::move(mock_video_decoder_client);

  auxiliary_endpoints->media_log_remote =
      mock_video_decoder.TakeMediaLogRemote();
  auxiliary_endpoints->mock_media_log = std::move(mock_media_log);

  auxiliary_endpoints->video_frame_handle_releaser_remote =
      std::move(video_frame_handle_releaser_remote);
  auxiliary_endpoints->mock_video_frame_handle_releaser =
      mock_video_decoder.TakeVideoFrameHandleReleaser();

  auxiliary_endpoints->mojo_decoder_buffer_writer =
      std::move(mojo_decoder_buffer_writer);
  auxiliary_endpoints->mojo_decoder_buffer_reader =
      mock_video_decoder.TakeMojoDecoderBufferReader();

  return auxiliary_endpoints;
}

class OOPVideoDecoderServiceTest : public testing::Test {
 public:
  OOPVideoDecoderServiceTest()
      : oop_video_decoder_factory_service_(gpu::GpuFeatureInfo(), nullptr) {
    oop_video_decoder_factory_service_
        .SetVideoDecoderCreationCallbackForTesting(
            video_decoder_creation_cb_.Get());
  }

  OOPVideoDecoderServiceTest(const OOPVideoDecoderServiceTest&) = delete;
  OOPVideoDecoderServiceTest& operator=(const OOPVideoDecoderServiceTest&) =
      delete;
  ~OOPVideoDecoderServiceTest() override = default;

  void SetUp() override {
    mojo::PendingReceiver<mojom::InterfaceFactory>
        video_decoder_factory_receiver;
    video_decoder_factory_remote_ = mojo::Remote<mojom::InterfaceFactory>(
        video_decoder_factory_receiver.InitWithNewPipeAndPassRemote());
    oop_video_decoder_factory_service_.BindReceiver(
        std::move(video_decoder_factory_receiver),
        /*disconnect_cb=*/base::DoNothing());
    ASSERT_TRUE(video_decoder_factory_remote_.is_connected());
  }

 protected:
  mojo::Remote<mojom::VideoDecoder> CreateVideoDecoder(
      std::unique_ptr<StrictMock<MockVideoDecoder>> dst_video_decoder,
      mojo::PendingRemote<mojom::VideoDecoderTracker> tracker) {
    // Each CreateVideoDecoder() should result in exactly one call to the
    // video decoder creation callback, i.e., the
    // OOPVideoDecoderFactoryService should not re-use mojom::VideoDecoder
    // implementation instances.
    EXPECT_CALL(video_decoder_creation_cb_, Run(_, _))
        .WillOnce(Return(ByMove(std::move(dst_video_decoder))));
    mojo::PendingReceiver<mojom::VideoDecoder> video_decoder_receiver;
    mojo::Remote<mojom::VideoDecoder> video_decoder_remote(
        video_decoder_receiver.InitWithNewPipeAndPassRemote());
    video_decoder_factory_remote_->CreateVideoDecoderWithTracker(
        std::move(video_decoder_receiver), std::move(tracker));
    video_decoder_factory_remote_.FlushForTesting();
    if (!Mock::VerifyAndClearExpectations(&video_decoder_creation_cb_)) {
      return {};
    }
    return video_decoder_remote;
  }

  base::test::TaskEnvironment task_environment_;
  StrictMock<base::MockRepeatingCallback<std::unique_ptr<
      mojom::VideoDecoder>(MojoMediaClient*, MojoCdmServiceContext*)>>
      video_decoder_creation_cb_;
  OOPVideoDecoderFactoryService oop_video_decoder_factory_service_;
  mojo::Remote<mojom::InterfaceFactory> video_decoder_factory_remote_;
  mojo::Remote<mojom::VideoDecoder> video_decoder_remote_;
};

// Tests that we can create multiple VideoDecoder implementation instances
// through the InterfaceFactory and that they can exist concurrently.
TEST_F(OOPVideoDecoderServiceTest, FactoryCanCreateVideoDecoders) {
  std::vector<mojo::Remote<mojom::VideoDecoder>> video_decoder_remotes;
  constexpr size_t kNumConcurrentDecoders = 5u;
  for (size_t i = 0u; i < kNumConcurrentDecoders; i++) {
    auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
    auto video_decoder_remote =
        CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
    video_decoder_remotes.push_back(std::move(video_decoder_remote));
  }
  for (const auto& remote : video_decoder_remotes) {
    ASSERT_TRUE(remote.is_bound());
    ASSERT_TRUE(remote.is_connected());
  }
}

// Tests that a call to mojom::VideoDecoder::Construct() gets routed
// correctly to the underlying mojom::VideoDecoder.
TEST_F(OOPVideoDecoderServiceTest, VideoDecoderCanBeConstructed) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  ASSERT_TRUE(ConstructVideoDecoder(video_decoder_remote,
                                    *mock_video_decoder_raw,
                                    /*expect_construct_call=*/true));
}

// Tests that if two calls to mojom::VideoDecoder::Construct() are made,
// only one is routed to the underlying mojom::VideoDecoder.
TEST_F(OOPVideoDecoderServiceTest, VideoDecoderCannotBeConstructedTwice) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  EXPECT_TRUE(ConstructVideoDecoder(video_decoder_remote,
                                    *mock_video_decoder_raw,
                                    /*expect_construct_call=*/true));
  EXPECT_TRUE(ConstructVideoDecoder(video_decoder_remote,
                                    *mock_video_decoder_raw,
                                    /*expect_construct_call=*/false));
}

// Tests that a call to mojom::VideoDecoder::GetSupportedConfigs() gets
// routed correctly to the underlying mojom::VideoDecoder. Also tests that the
// underlying mojom::VideoDecoder's reply gets routed correctly back to the
// client.
TEST_F(OOPVideoDecoderServiceTest, VideoDecoderCanGetSupportedConfigs) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());

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

  video_decoder_remote->GetSupportedConfigs(
      get_supported_configs_cb_to_send.Get());
  video_decoder_remote.FlushForTesting();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(mock_video_decoder_raw));

  std::move(received_get_supported_configs_cb)
      .Run(supported_configs_to_reply_with, kDecoderTypeToReplyWith);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(received_supported_configs, supported_configs_to_reply_with);
}

// Tests that a call to mojom::VideoDecoder::Initialize() gets
// routed correctly to the underlying mojom::VideoDecoder as an Initialize()
// call. Also tests that when the underlying mojom::VideoDecoder calls the
// initialization callback, the call gets routed to the client.
TEST_F(OOPVideoDecoderServiceTest, VideoDecoderCanBeInitialized) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  auto auxiliary_endpoints =
      ConstructVideoDecoder(video_decoder_remote, *mock_video_decoder_raw,
                            /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);

  const VideoDecoderConfig config_to_send = CreateValidVideoDecoderConfig();
  VideoDecoderConfig received_config;
  constexpr bool kLowDelay = true;
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
              Initialize(/*config=*/_, kLowDelay,
                         /*cdm=*/Property(&mojom::CdmPtr::is_null, IsTrue()),
                         /*callback=*/_))
      .WillOnce([&](const VideoDecoderConfig& config, bool low_delay,
                    mojom::CdmPtr cdm,
                    mojom::VideoDecoder::InitializeCallback callback) {
        received_config = config;
        received_initialize_cb = std::move(callback);
      });
  EXPECT_CALL(initialize_cb_to_send,
              Run(kDecoderStatus, kNeedsBitstreamConversion, kMaxDecodeRequests,
                  kDecoderType, /*needs_transcryption=*/false));
  video_decoder_remote->Initialize(config_to_send, kLowDelay, nullptr,
                                   initialize_cb_to_send.Get());
  video_decoder_remote.FlushForTesting();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(mock_video_decoder_raw));

  std::move(received_initialize_cb)
      .Run(kDecoderStatus, kNeedsBitstreamConversion, kMaxDecodeRequests,
           kDecoderType, /*needs_transcryption=*/false);
  task_environment_.RunUntilIdle();
}

// Tests that the OOPVideoDecoderService rejects a call to
// mojom::VideoDecoder::Initialize() before mojom::VideoDecoder::Construct()
// gets called.
TEST_F(OOPVideoDecoderServiceTest,
       VideoDecoderCannotBeInitializedBeforeConstruction) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());

  const VideoDecoderConfig config_to_send = CreateValidVideoDecoderConfig();
  constexpr bool kLowDelay = true;
  StrictMock<base::MockOnceCallback<void(
      const media::DecoderStatus& status, bool needs_bitstream_conversion,
      int32_t max_decode_requests, VideoDecoderType decoder_type,
      bool needs_transcryption)>>
      initialize_cb_to_send;

  EXPECT_CALL(initialize_cb_to_send,
              Run(DecoderStatus(DecoderStatus::Codes::kFailedToCreateDecoder),
                  /*needs_bitstream_conversion=*/false,
                  /*max_decode_requests=*/1, VideoDecoderType::kUnknown,
                  /*needs_transcryption=*/false));
  video_decoder_remote->Initialize(config_to_send, kLowDelay, nullptr,
                                   initialize_cb_to_send.Get());
  video_decoder_remote.FlushForTesting();
}

// Tests that a call to mojom::VideoDecoder::Decode() gets routed
// correctly to the underlying mojom::VideoDecoder and that the data pipe is
// plumbed correctly. Also tests that when the underlying mojom::VideoDecoder
// calls the decode callback, the call gets routed to the client.
TEST_F(OOPVideoDecoderServiceTest, VideoDecoderCanDecode) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  auto auxiliary_endpoints =
      ConstructVideoDecoder(video_decoder_remote, *mock_video_decoder_raw,
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
  mojom::DecoderBufferPtr mojo_decoder_buffer =
      auxiliary_endpoints->mojo_decoder_buffer_writer->WriteDecoderBuffer(
          decoder_buffer_to_send);
  ASSERT_TRUE(mojo_decoder_buffer);
  video_decoder_remote->Decode(std::move(mojo_decoder_buffer),
                               decode_cb_to_send.Get());
  video_decoder_remote.FlushForTesting();
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

// Tests that the OOPVideoDecoderService rejects a call to
// mojom::VideoDecoder::Decode() before
// mojom::VideoDecoder::Construct() gets called.
TEST_F(OOPVideoDecoderServiceTest, VideoDecoderCannotDecodeBeforeConstruction) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());

  constexpr uint8_t kEncodedData[] = {1, 2, 3};
  scoped_refptr<DecoderBuffer> decoder_buffer_to_send =
      DecoderBuffer::CopyFrom(kEncodedData);
  ASSERT_TRUE(decoder_buffer_to_send);
  StrictMock<base::MockOnceCallback<void(const media::DecoderStatus& status)>>
      decode_cb_to_send;

  EXPECT_CALL(decode_cb_to_send,
              Run(DecoderStatus(DecoderStatus::Codes::kFailedToCreateDecoder)));
  mojom::DecoderBufferPtr mojo_decoder_buffer =
      mojom::DecoderBuffer::From(*decoder_buffer_to_send);
  ASSERT_TRUE(mojo_decoder_buffer);
  video_decoder_remote->Decode(std::move(mojo_decoder_buffer),
                               decode_cb_to_send.Get());
  video_decoder_remote.FlushForTesting();
}

// Tests that a call to mojom::VideoDecoder::Reset() gets routed
// correctly to the underlying mojom::VideoDecoder. Also tests that when the
// underlying mojom::VideoDecoder calls the reset callback, the call gets routed
// to the client.
TEST_F(OOPVideoDecoderServiceTest, VideoDecoderCanBeReset) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  auto auxiliary_endpoints =
      ConstructVideoDecoder(video_decoder_remote, *mock_video_decoder_raw,
                            /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);

  StrictMock<base::MockOnceCallback<void()>> reset_cb_to_send;
  mojom::VideoDecoder::ResetCallback received_reset_cb;

  EXPECT_CALL(*mock_video_decoder_raw, Reset(/*callback=*/_))
      .WillOnce([&](mojom::VideoDecoder::ResetCallback callback) {
        received_reset_cb = std::move(callback);
      });
  EXPECT_CALL(reset_cb_to_send, Run());
  video_decoder_remote->Reset(reset_cb_to_send.Get());
  video_decoder_remote.FlushForTesting();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(mock_video_decoder_raw));

  std::move(received_reset_cb).Run();
  task_environment_.RunUntilIdle();
}

// Tests that the OOPVideoDecoderService doesn't route a
// mojom::VideoDecoder::Reset() call to the underlying
// mojom::VideoDecoder before mojom::VideoDecoder::Construct() gets
// called and that it just calls the reset callback.
TEST_F(OOPVideoDecoderServiceTest,
       VideoDecoderCannotBeResetBeforeConstruction) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());

  StrictMock<base::MockOnceCallback<void()>> reset_cb_to_send;

  EXPECT_CALL(reset_cb_to_send, Run());
  video_decoder_remote->Reset(reset_cb_to_send.Get());
  video_decoder_remote.FlushForTesting();
}

// Tests that a call to
// mojom::VideoFrameHandleReleaser::ReleaseVideoFrame() gets routed
// correctly to the underlying mojom::VideoFrameHandleReleaser.
TEST_F(OOPVideoDecoderServiceTest, VideoFramesCanBeReleased) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  auto auxiliary_endpoints =
      ConstructVideoDecoder(video_decoder_remote, *mock_video_decoder_raw,
                            /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->video_frame_handle_releaser_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_video_frame_handle_releaser);

  const base::UnguessableToken release_token_to_send =
      base::UnguessableToken::Create();
  const std::optional<gpu::SyncToken> expected_release_sync_token =
      std::nullopt;

  EXPECT_CALL(
      *auxiliary_endpoints->mock_video_frame_handle_releaser,
      ReleaseVideoFrame(release_token_to_send, expected_release_sync_token));
  auxiliary_endpoints->video_frame_handle_releaser_remote->ReleaseVideoFrame(
      release_token_to_send, /*release_sync_token=*/{});
  auxiliary_endpoints->video_frame_handle_releaser_remote.FlushForTesting();
}

TEST_F(OOPVideoDecoderServiceTest,
       VideoDecoderClientReceivesOnVideoFrameDecodedEvent) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  auto auxiliary_endpoints =
      ConstructVideoDecoder(video_decoder_remote, *mock_video_decoder_raw,
                            /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->video_decoder_client_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_video_decoder_client);

  const std::optional<base::UnguessableToken> token_for_release =
      base::UnguessableToken::Create();
  scoped_refptr<VideoFrame> video_frame_to_send = CreateTestNV12VideoFrame();
  ASSERT_TRUE(video_frame_to_send);
  scoped_refptr<VideoFrame> video_frame_received;
  constexpr bool kCanReadWithoutStalling = true;
  EXPECT_CALL(
      *auxiliary_endpoints->mock_video_decoder_client,
      OnVideoFrameDecoded(_, kCanReadWithoutStalling, token_for_release))
      .WillOnce(WithArgs<0>(
          [&video_frame_received](const scoped_refptr<VideoFrame>& frame) {
            video_frame_received = std::move(frame);
          }));
  auxiliary_endpoints->video_decoder_client_remote->OnVideoFrameDecoded(
      video_frame_to_send, kCanReadWithoutStalling, token_for_release);
  auxiliary_endpoints->video_decoder_client_remote.FlushForTesting();
  ASSERT_TRUE(video_frame_received);
  EXPECT_FALSE(video_frame_received->metadata().end_of_stream);
  EXPECT_TRUE(video_frame_received->metadata().read_lock_fences_enabled);
  EXPECT_TRUE(video_frame_received->metadata().power_efficient);
  EXPECT_TRUE(video_frame_received->metadata().allow_overlay);
}

// Tests that a mojom::VideoDecoderClient::OnWaiting() call originating from the
// underlying mojom::VideoDecoder gets forwarded to the
// mojom::VideoDecoderClient correctly.
TEST_F(OOPVideoDecoderServiceTest, VideoDecoderClientReceivesOnWaitingEvent) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  auto auxiliary_endpoints =
      ConstructVideoDecoder(video_decoder_remote, *mock_video_decoder_raw,
                            /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->video_decoder_client_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_video_decoder_client);

  constexpr WaitingReason kWaitingReason = WaitingReason::kNoDecryptionKey;
  EXPECT_CALL(*auxiliary_endpoints->mock_video_decoder_client,
              OnWaiting(kWaitingReason));
  auxiliary_endpoints->video_decoder_client_remote->OnWaiting(kWaitingReason);
  auxiliary_endpoints->video_decoder_client_remote.FlushForTesting();
}

// Tests that a mojom::MediaLog::AddLogRecord() call originating from the
// underlying mojom::VideoDecoder gets forwarded to the mojom::MediaLog
// correctly.
TEST_F(OOPVideoDecoderServiceTest,
       VideoDecoderClientReceivesAddLogRecordEvent) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto video_decoder_remote =
      CreateVideoDecoder(std::move(mock_video_decoder), /*tracker=*/{});
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());
  auto auxiliary_endpoints =
      ConstructVideoDecoder(video_decoder_remote, *mock_video_decoder_raw,
                            /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->media_log_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_media_log);

  MediaLogRecord media_log_record_to_send;
  media_log_record_to_send.id = MediaPlayerLoggingID(2);
  media_log_record_to_send.type = MediaLogRecord::Type::kMediaStatus;
  media_log_record_to_send.params.Set("Test", "Value");
  media_log_record_to_send.time = base::TimeTicks::Now();

  EXPECT_CALL(*auxiliary_endpoints->mock_media_log,
              AddLogRecord(media_log_record_to_send));
  auxiliary_endpoints->media_log_remote->AddLogRecord(media_log_record_to_send);
  auxiliary_endpoints->media_log_remote.FlushForTesting();
}

// Tests that a VideoDecoderTracker can be used to know when the remote
// VideoDecoder implementation dies.
TEST_F(OOPVideoDecoderServiceTest,
       VideoDecoderTrackerDisconnectsWhenVideoDecoderDies) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();

  MockVideoDecoderTracker tracker;
  mojo::Receiver<mojom::VideoDecoderTracker> tracker_receiver(&tracker);
  mojo::PendingRemote<mojom::VideoDecoderTracker> tracker_remote =
      tracker_receiver.BindNewPipeAndPassRemote();
  StrictMock<base::MockOnceCallback<void()>> tracker_disconnect_cb;
  tracker_receiver.set_disconnect_handler(tracker_disconnect_cb.Get());

  auto video_decoder_remote = CreateVideoDecoder(std::move(mock_video_decoder),
                                                 std::move(tracker_remote));
  ASSERT_TRUE(video_decoder_remote.is_bound());
  ASSERT_TRUE(video_decoder_remote.is_connected());

  // Until now, nothing in particular should happen.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(&tracker_disconnect_cb));

  // Once we reset the |video_decoder_remote|, the VideoDecoder
  // implementation should die and the |tracker| should get disconnected.
  EXPECT_CALL(tracker_disconnect_cb, Run());
  video_decoder_remote.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace media
