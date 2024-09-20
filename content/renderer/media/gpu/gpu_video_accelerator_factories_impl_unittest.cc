// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"

#include <GLES2/gl2.h>

#include <cstddef>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/renderer/media/codec_factory.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "media/base/decoder.h"
#include "media/base/media_util.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
#include "content/renderer/media/codec_factory_mojo.h"
#include "media/filters/fake_video_decoder.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_video_decoder_service.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "content/renderer/media/codec_factory_fuchsia.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#endif

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

constexpr gfx::Size kCodedSize(320, 240);
constexpr gfx::Rect kVisibleRect(320, 240);
constexpr gfx::Size kNaturalSize(320, 240);

const media::SupportedVideoDecoderConfig kH264MaxSupportedVideoDecoderConfig =
    media::SupportedVideoDecoderConfig(
        media::VideoCodecProfile::H264PROFILE_MIN,
        media::VideoCodecProfile::H264PROFILE_MAX,
        media::kDefaultSwDecodeSizeMin,
        media::kDefaultSwDecodeSizeMax,
        true,
        false);

const media::VideoDecoderConfig kH264BaseConfig(
    media::VideoCodec::kH264,
    media::H264PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    kCodedSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const media::VideoDecoderConfig kVP9BaseConfig(
    media::VideoCodec::kVP9,
    media::VP9PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    kCodedSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

}  // namespace

class TestGpuChannelHost : public gpu::GpuChannelHost {
 public:
  explicit TestGpuChannelHost(gpu::mojom::GpuChannel& gpu_channel)
      : GpuChannelHost(0 /* channel_id */,
                       gpu::GPUInfo(),
                       gpu::GpuFeatureInfo(),
                       gpu::SharedImageCapabilities(),
                       mojo::ScopedMessagePipeHandle(
                           mojo::MessagePipeHandle(mojo::kInvalidHandleValue))),
        gpu_channel_(gpu_channel) {}

  gpu::mojom::GpuChannel& GetGpuChannel() override { return *gpu_channel_; }

 protected:
  ~TestGpuChannelHost() override = default;
  const raw_ref<gpu::mojom::GpuChannel> gpu_channel_;
};

class MockOverlayInfoCbHandler {
 public:
  MOCK_METHOD2(Call, void(bool, media::ProvideOverlayInfoCB));
};

class MockContextProviderCommandBuffer
    : public viz::ContextProviderCommandBuffer {
 public:
  explicit MockContextProviderCommandBuffer(
      scoped_refptr<gpu::GpuChannelHost> channel)
      : viz::ContextProviderCommandBuffer(
            std::move(channel),
            content::kGpuStreamIdDefault,
            content::kGpuStreamPriorityDefault,
            gpu::kNullSurfaceHandle,
            GURL(),
            false,
            true,
            gpu::SharedMemoryLimits(),
            gpu::ContextCreationAttribs(),
            viz::command_buffer_metrics::ContextType::FOR_TESTING) {}

  MOCK_METHOD(gpu::CommandBufferProxyImpl*,
              GetCommandBufferProxy,
              (),
              (override));

  // ContextProvider / RasterContextProvider implementation.
  MOCK_METHOD(gpu::ContextResult, BindToCurrentSequence, (), (override));
  MOCK_METHOD(gpu::gles2::GLES2Interface*, ContextGL, (), (override));
  MOCK_METHOD(gpu::raster::RasterInterface*, RasterInterface, (), (override));
  MOCK_METHOD(gpu::ContextSupport*, ContextSupport, (), (override));
  MOCK_METHOD(class GrDirectContext*, GrContext, (), (override));
  MOCK_METHOD(gpu::SharedImageInterface*, SharedImageInterface, (), (override));
  MOCK_METHOD(viz::ContextCacheController*, CacheController, (), (override));
  MOCK_METHOD(base::Lock*, GetLock, (), (override));
  MOCK_METHOD(gpu::Capabilities&, ContextCapabilities, (), (const, override));
  MOCK_METHOD(gpu::GpuFeatureInfo&, GetGpuFeatureInfo, (), (const, override));
  MOCK_METHOD(void, AddObserver, (viz::ContextLostObserver*), (override));
  MOCK_METHOD(void, RemoveObserver, (viz::ContextLostObserver*), (override));

  // base::trace_event::MemoryDumpProvider implementation.
  MOCK_METHOD(bool,
              OnMemoryDump,
              (const base::trace_event::MemoryDumpArgs&,
               base::trace_event::ProcessMemoryDump*),
              (override));

 protected:
  ~MockContextProviderCommandBuffer() override = default;
};

class MockGLESInterface : public gpu::gles2::GLES2InterfaceStub {
 public:
  MOCK_METHOD(GLenum, GetGraphicsResetStatusKHR, ());
};

class FakeVEAProviderImpl
    : public media::mojom::VideoEncodeAcceleratorProvider {
 public:
  ~FakeVEAProviderImpl() override = default;

  void Bind(mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
                receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void SetVideoEncodeAcceleratorSupportedProfiles(
      std::vector<media::VideoEncodeAccelerator::SupportedProfile>
          supported_profile) {
    supported_profile_ = supported_profile;
  }
  // media::mojom::VideoEncodeAcceleratorProvider impl.
  void CreateVideoEncodeAccelerator(
      media::mojom::EncodeCommandBufferIdPtr command_buffer_id,
      mojo::PendingReceiver<media::mojom::VideoEncodeAccelerator> receiver)
      override {}
  void GetVideoEncodeAcceleratorSupportedProfiles(
      GetVideoEncodeAcceleratorSupportedProfilesCallback callback) override {
    std::move(callback).Run(supported_profile_);
  }

 private:
  mojo::Receiver<media::mojom::VideoEncodeAcceleratorProvider> receiver_{this};
  std::vector<media::VideoEncodeAccelerator::SupportedProfile>
      supported_profile_;
};

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
// Client to MojoVideoDecoderService vended by FakeInterfaceFactory. Creates a
// FakeGpuVideoDecoder when requested.
class FakeMojoMediaClient : public media::MojoMediaClient {
 public:
  void SetSupportedVideoDecoderConfigs(
      media::SupportedVideoDecoderConfigs configs) {
    supported_video_decoder_configs_ = configs;
  }

  // MojoMediaClient implementation.
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      media::MediaLog* media_log,
      media::mojom::CommandBufferIdPtr command_buffer_id,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          oop_video_decoder) override {
    return std::make_unique<media::FakeVideoDecoder>(
        0 /* decoder_id */, 0 /* decoding_delay */,
        13 /* max_parallel_decoding_requests */, media::BytesDecodedCB());
  }
  media::SupportedVideoDecoderConfigs GetSupportedVideoDecoderConfigs()
      override {
    return supported_video_decoder_configs_;
  }
  media::VideoDecoderType GetDecoderImplementationType() override {
    return media::VideoDecoderType::kTesting;
  }

 private:
  media::SupportedVideoDecoderConfigs supported_video_decoder_configs_;
};

// Other end of remote InterfaceFactory requested by VideoDecoderBroker. Used
// to create our (fake) media::mojom::VideoDecoder.
class FakeInterfaceFactory : public media::mojom::InterfaceFactory {
 public:
  FakeInterfaceFactory() = default;
  ~FakeInterfaceFactory() override = default;

  void Bind(mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(base::BindOnce(
        &FakeInterfaceFactory::OnConnectionError, base::Unretained(this)));
  }

  void SetSupportedVideoDecoderConfigs(
      media::SupportedVideoDecoderConfigs configs) {
    mojo_media_client_.SetSupportedVideoDecoderConfigs(configs);
  }

  // Implement this one interface from mojom::InterfaceFactory. Using the real
  // MojoVideoDecoderService allows us to reuse buffer conversion code. The
  // FakeMojoMediaClient will create a FakeGpuVideoDecoder.
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder) override {
    video_decoder_receivers_.Add(
        std::make_unique<media::MojoVideoDecoderService>(
            &mojo_media_client_, &cdm_service_context_,
            mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>()),
        std::move(receiver));
  }

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
          video_decoder) override {
    // TODO(b/327268445): we'll need to complete this for GTFO OOP-VD testing.
  }
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  // Stub out other mojom::InterfaceFactory interfaces.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) override {}
  void CreateAudioEncoder(
      mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) override {}
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#endif
#if BUILDFLAG(IS_ANDROID)
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) override {}
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote) override {}
#endif  // BUILDFLAG(IS_WIN)
  void CreateCdm(const media::CdmConfig& cdm_config,
                 CreateCdmCallback callback) override {}

 private:
  void OnConnectionError() { receiver_.reset(); }

  media::MojoCdmServiceContext cdm_service_context_;
  FakeMojoMediaClient mojo_media_client_;
  mojo::Receiver<media::mojom::InterfaceFactory> receiver_{this};
  mojo::UniqueReceiverSet<media::mojom::VideoDecoder> video_decoder_receivers_;
};
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(IS_FUCHSIA)
class FakeFuchsiaMediaCodecProvide
    : public media::mojom::FuchsiaMediaCodecProvider {
 public:
  ~FakeFuchsiaMediaCodecProvide() override = default;

  void Bind(
      mojo::PendingReceiver<media::mojom::FuchsiaMediaCodecProvider> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void SetSupportedVideoDecoderConfigs(
      media::SupportedVideoDecoderConfigs configs) {
    supported_video_decoder_configs_ = configs;
  }

  // media::mojom::FuchsiaMediaCodecProvider implementation.
  void CreateVideoDecoder(
      media::VideoCodec codec,
      media::mojom::VideoDecoderSecureMemoryMode secure_mode,
      fidl::InterfaceRequest<fuchsia::media::StreamProcessor>
          stream_processor_request) final {
    ADD_FAILURE() << "Not implemented.";
  }

  void GetSupportedVideoDecoderConfigs(
      GetSupportedVideoDecoderConfigsCallback callback) final {
    std::move(callback).Run(supported_video_decoder_configs_);
  }

 private:
  media::SupportedVideoDecoderConfigs supported_video_decoder_configs_;
  mojo::Receiver<media::mojom::FuchsiaMediaCodecProvider> receiver_{this};
};
#endif  // BUILDFLAG(IS_FUCHSIA)

class GpuVideoAcceleratorFactoriesImplTest : public testing::Test {
 public:
  GpuVideoAcceleratorFactoriesImplTest()
      : gpu_channel_host_(
            base::MakeRefCounted<TestGpuChannelHost>(mock_gpu_channel_)),
        mock_context_provider_(
            base::MakeRefCounted<NiceMock<MockContextProviderCommandBuffer>>(
                gpu_channel_host_)) {}
  ~GpuVideoAcceleratorFactoriesImplTest() override = default;

  void SetUp() override {
    MockGpuChannel();
    MockContextProvider();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(testing::Mock::VerifyAndClear(&mock_context_provider_));
    ASSERT_TRUE(testing::Mock::VerifyAndClear(&mock_context_gl_));
    ASSERT_TRUE(testing::Mock::VerifyAndClear(&mock_gpu_channel_));
    gpu_command_buffer_proxy_.reset();
    mock_context_provider_.reset();
    gpu_channel_host_.reset();
  }

  void MockGpuChannel() {
    // Simulate success, since we're not actually talking to the service
    // in this test suite.
    ON_CALL(mock_gpu_channel_, CreateCommandBuffer(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(
            [&](gpu::mojom::CreateCommandBufferParamsPtr params,
                int32_t routing_id, base::UnsafeSharedMemoryRegion shared_state,
                mojo::PendingAssociatedReceiver<gpu::mojom::CommandBuffer>
                    receiver,
                mojo::PendingAssociatedRemote<gpu::mojom::CommandBufferClient>
                    client,
                gpu::ContextResult* result, gpu::Capabilities* capabilities,
                gpu::GLCapabilities* gl_capabilities) -> bool {
              // There's no real GpuChannel pipe for this endpoint to use, so
              // associate it with a dedicated pipe for these tests. This
              // allows the CommandBufferProxyImpl to make calls on its
              // CommandBuffer endpoint.
              receiver.EnableUnassociatedUsage();
              *result = gpu::ContextResult::kSuccess;
              return true;
            }));
    ON_CALL(mock_gpu_channel_, GetChannelToken(_))
        .WillByDefault(Invoke(
            [&](gpu::MockGpuChannel::GetChannelTokenCallback callback) -> void {
              std::move(callback).Run(base::UnguessableToken::Create());
            }));
  }

  void MockContextProvider() {
    ON_CALL(*mock_context_provider_, BindToCurrentSequence())
        .WillByDefault(Return(gpu::ContextResult::kSuccess));
    ON_CALL(mock_context_gl_, GetGraphicsResetStatusKHR())
        .WillByDefault(Return(GL_NO_ERROR));
    ON_CALL(*mock_context_provider_, ContextGL())
        .WillByDefault(Return(&mock_context_gl_));

    gpu_command_buffer_proxy_ = std::make_unique<gpu::CommandBufferProxyImpl>(
        gpu_channel_host_, content::kGpuStreamIdDefault,
        task_environment_.GetMainThreadTaskRunner());
    gpu_command_buffer_proxy_->Initialize(
        gpu::kNullSurfaceHandle, nullptr, content::kGpuStreamPriorityDefault,
        gpu::ContextCreationAttribs(), GURL());
    ON_CALL(*mock_context_provider_, GetCommandBufferProxy())
        .WillByDefault(Return(gpu_command_buffer_proxy_.get()));
  }

  std::unique_ptr<CodecFactory> CreateCodecFactory(
      scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
      bool enable_video_decode_accelerator,
      bool enable_video_encode_accelerator) {
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider;
    fake_vea_provider_.Bind(vea_provider.InitWithNewPipeAndPassReceiver());

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
    mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory;
    fake_media_codec_provider_.Bind(
        interface_factory.InitWithNewPipeAndPassReceiver());
    return std::make_unique<CodecFactoryMojo>(
        task_environment_.GetMainThreadTaskRunner(),
        std::move(context_provider), enable_video_decode_accelerator,
        enable_video_encode_accelerator, std::move(vea_provider),
        std::move(interface_factory));
#elif BUILDFLAG(IS_FUCHSIA)
    mojo::PendingRemote<media::mojom::FuchsiaMediaCodecProvider>
        media_codec_provider;
    fake_media_codec_provider_.Bind(
        media_codec_provider.InitWithNewPipeAndPassReceiver());
    return std::make_unique<CodecFactoryFuchsia>(
        task_environment_.GetMainThreadTaskRunner(),
        std::move(context_provider), enable_video_decode_accelerator,
        enable_video_encode_accelerator, std::move(vea_provider),
        std::move(media_codec_provider));
#else
    return std::make_unique<CodecFactoryDefault>(
        task_environment_.GetMainThreadTaskRunner(),
        std::move(context_provider), enable_video_decode_accelerator,
        enable_video_encode_accelerator, std::move(vea_provider));
#endif
  }

  std::unique_ptr<GpuVideoAcceleratorFactoriesImpl>
  CreateGpuVideoAcceleratorFactories(bool enable_video_decode_accelerator,
                                     bool enable_video_encode_accelerator) {
    std::unique_ptr<CodecFactory> codec_factory = CreateCodecFactory(
        mock_context_provider_, enable_video_decode_accelerator,
        enable_video_encode_accelerator);
    auto gpu_factories = GpuVideoAcceleratorFactoriesImpl::CreateForTesting(
        gpu_channel_host_, task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(), mock_context_provider_,
        std::move(codec_factory), &gpu_memory_buffer_manager_,
        true, /* enable_video_gpu_memory_buffers */
        true, /* enable_media_stream_gpu_memory_buffers */
        enable_video_decode_accelerator, enable_video_encode_accelerator);

    // Wait until all async IO messages (e.g. Mojo and FIDL) to be delieved
    // and handled.
    task_environment_.RunUntilIdle();

    return gpu_factories;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  NiceMock<gpu::MockGpuChannel> mock_gpu_channel_;
  NiceMock<MockGLESInterface> mock_context_gl_;
  gpu::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  scoped_refptr<TestGpuChannelHost> gpu_channel_host_;
  scoped_refptr<MockContextProviderCommandBuffer> mock_context_provider_;
  std::unique_ptr<gpu::CommandBufferProxyImpl> gpu_command_buffer_proxy_;

  FakeVEAProviderImpl fake_vea_provider_;

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  FakeInterfaceFactory fake_media_codec_provider_;
#elif BUILDFLAG(IS_FUCHSIA)
  FakeFuchsiaMediaCodecProvide fake_media_codec_provider_;
#endif
};

TEST_F(GpuVideoAcceleratorFactoriesImplTest, VideoDecoderAcceleratorDisabled) {
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(false, false);

  EXPECT_FALSE(
      gpu_video_accelerator_factories->IsGpuVideoDecodeAcceleratorEnabled());
  EXPECT_TRUE(gpu_video_accelerator_factories->IsDecoderSupportKnown());
  EXPECT_EQ(gpu_video_accelerator_factories->IsDecoderConfigSupported(
                kH264BaseConfig),
            media::GpuVideoAcceleratorFactories::Supported::kFalse);
  EXPECT_EQ(gpu_video_accelerator_factories->GetDecoderType(),
            media::VideoDecoderType::kUnknown);

  ASSERT_TRUE(
      testing::Mock::VerifyAndClearExpectations(&mock_context_provider_));
}

TEST_F(GpuVideoAcceleratorFactoriesImplTest, VideoEncoderAcceleratorDisabled) {
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(false, false);

  EXPECT_FALSE(
      gpu_video_accelerator_factories->IsGpuVideoEncodeAcceleratorEnabled());
  EXPECT_TRUE(gpu_video_accelerator_factories->IsEncoderSupportKnown());
}

TEST_F(GpuVideoAcceleratorFactoriesImplTest, EncoderConfigsIsSupported) {
  fake_vea_provider_.SetVideoEncodeAcceleratorSupportedProfiles(
      {media::VideoEncodeAccelerator::SupportedProfile(
          media::VideoCodecProfile::VP9PROFILE_MAX, kCodedSize)});
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(false, true);

  EXPECT_TRUE(
      gpu_video_accelerator_factories->IsGpuVideoEncodeAcceleratorEnabled());
  EXPECT_TRUE(gpu_video_accelerator_factories->IsEncoderSupportKnown());
  base::test::TestFuture<void> future;
  gpu_video_accelerator_factories->NotifyEncoderSupportKnown(
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  auto supported_profiles = gpu_video_accelerator_factories
                                ->GetVideoEncodeAcceleratorSupportedProfiles();
  EXPECT_TRUE(supported_profiles.has_value());
  EXPECT_EQ(supported_profiles->size(), static_cast<size_t>(1));
}

TEST_F(GpuVideoAcceleratorFactoriesImplTest, EncoderConfigsIsNotSupported) {
  fake_vea_provider_.SetVideoEncodeAcceleratorSupportedProfiles({});
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(false, true);

  EXPECT_TRUE(
      gpu_video_accelerator_factories->IsGpuVideoEncodeAcceleratorEnabled());
  EXPECT_TRUE(gpu_video_accelerator_factories->IsEncoderSupportKnown());
  base::test::TestFuture<void> future;
  gpu_video_accelerator_factories->NotifyEncoderSupportKnown(
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  auto supported_profiles = gpu_video_accelerator_factories
                                ->GetVideoEncodeAcceleratorSupportedProfiles();
  EXPECT_TRUE(supported_profiles.has_value());
  EXPECT_TRUE(supported_profiles->empty());
}

TEST_F(GpuVideoAcceleratorFactoriesImplTest, CreateVideoEncodeAccelerator) {
  fake_vea_provider_.SetVideoEncodeAcceleratorSupportedProfiles(
      {media::VideoEncodeAccelerator::SupportedProfile(
          media::VideoCodecProfile::VP9PROFILE_MAX, kCodedSize)});
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(false, true);

  EXPECT_NE(gpu_video_accelerator_factories->CreateVideoEncodeAccelerator(),
            nullptr);
}

#ifdef GTEST_HAS_DEATH_TEST
using GpuVideoAcceleratorFactoriesImplDeathTest =
    GpuVideoAcceleratorFactoriesImplTest;

TEST_F(GpuVideoAcceleratorFactoriesImplDeathTest,
       CreateVideoEncodeAcceleratorFailed) {
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(false, false);

  EXPECT_DCHECK_DEATH({
    EXPECT_EQ(gpu_video_accelerator_factories->CreateVideoEncodeAccelerator(),
              nullptr);
  });
}

TEST_F(GpuVideoAcceleratorFactoriesImplDeathTest, CreateVideoDecoderFailed) {
  testing::StrictMock<MockOverlayInfoCbHandler> cb_handler;
  media::RequestOverlayInfoCB mock_cb = base::BindRepeating(
      &MockOverlayInfoCbHandler::Call, base::Unretained(&cb_handler));
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(false, false);

  EXPECT_DCHECK_DEATH({
    EXPECT_EQ(gpu_video_accelerator_factories->CreateVideoDecoder(
                  nullptr, std::move(mock_cb)),
              nullptr);
  });
}
#endif  // GTEST_HAS_DEATH_TEST

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER) || BUILDFLAG(IS_FUCHSIA)
TEST_F(GpuVideoAcceleratorFactoriesImplTest, DecoderConfigIsSupported) {
  fake_media_codec_provider_.SetSupportedVideoDecoderConfigs(
      {kH264MaxSupportedVideoDecoderConfig});

  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(true, false);

  EXPECT_TRUE(
      gpu_video_accelerator_factories->IsGpuVideoDecodeAcceleratorEnabled());
  EXPECT_TRUE(gpu_video_accelerator_factories->IsDecoderSupportKnown());
  base::test::TestFuture<void> future;
  gpu_video_accelerator_factories->NotifyDecoderSupportKnown(
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(gpu_video_accelerator_factories->IsDecoderConfigSupported(
                kH264BaseConfig),
            media::GpuVideoAcceleratorFactories::Supported::kTrue);
  EXPECT_NE(gpu_video_accelerator_factories->GetDecoderType(),
            media::VideoDecoderType::kUnknown);
}

TEST_F(GpuVideoAcceleratorFactoriesImplTest, DecoderConfigIsNotSupported) {
  fake_media_codec_provider_.SetSupportedVideoDecoderConfigs(
      {kH264MaxSupportedVideoDecoderConfig});

  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(true, false);

  EXPECT_TRUE(
      gpu_video_accelerator_factories->IsGpuVideoDecodeAcceleratorEnabled());
  EXPECT_TRUE(gpu_video_accelerator_factories->IsDecoderSupportKnown());
  base::test::TestFuture<void> future;
  gpu_video_accelerator_factories->NotifyDecoderSupportKnown(
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(
      gpu_video_accelerator_factories->IsDecoderConfigSupported(kVP9BaseConfig),
      media::GpuVideoAcceleratorFactories::Supported::kFalse);
}

TEST_F(GpuVideoAcceleratorFactoriesImplTest, CreateVideoDecoder) {
  testing::StrictMock<MockOverlayInfoCbHandler> cb_handler;
  media::RequestOverlayInfoCB mock_cb = base::BindRepeating(
      &MockOverlayInfoCbHandler::Call, base::Unretained(&cb_handler));
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(true, false);

  EXPECT_NE(gpu_video_accelerator_factories->CreateVideoDecoder(
                nullptr, std::move(mock_cb)),
            nullptr);
}
#else
TEST_F(GpuVideoAcceleratorFactoriesImplTest, DefaultCodecFactory) {
  auto gpu_video_accelerator_factories =
      CreateGpuVideoAcceleratorFactories(false, false);

  EXPECT_FALSE(
      gpu_video_accelerator_factories->IsGpuVideoDecodeAcceleratorEnabled());
  EXPECT_TRUE(gpu_video_accelerator_factories->IsDecoderSupportKnown());
  base::test::TestFuture<void> future;
  gpu_video_accelerator_factories->NotifyDecoderSupportKnown(
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(gpu_video_accelerator_factories->IsDecoderConfigSupported(
                kH264BaseConfig),
            media::GpuVideoAcceleratorFactories::Supported::kFalse);
}
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER) || BUILDFLAG(IS_FUCHSIA)

}  // namespace content
