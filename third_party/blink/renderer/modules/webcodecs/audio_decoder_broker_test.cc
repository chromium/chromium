// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder_broker.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/sample_format.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/services/interface_factory_impl.h"
#include "media/mojo/services/mojo_audio_decoder_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using ::testing::_;
using ::testing::Return;

namespace blink {

namespace {

// Constants to specify the type of audio data used.
constexpr media::AudioCodec kCodec = media::AudioCodec::kVorbis;
constexpr media::SampleFormat kSampleFormat = media::kSampleFormatPlanarF32;
constexpr media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
constexpr int kChannels = 2;
constexpr int kSamplesPerSecond = 44100;
constexpr int kInputFramesChunk = 256;

// FakeAudioDecoder is very agreeable.
// - any configuration is supported
// - all decodes immediately succeed
// - non EOS decodes produce an output
// - reset immediately succeeds.
class FakeAudioDecoder : public media::MockAudioDecoder {
 public:
  FakeAudioDecoder() : MockAudioDecoder() {}
  ~FakeAudioDecoder() override = default;

  void Initialize(const media::AudioDecoderConfig& config,
                  media::CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const media::WaitingCB& waiting_cb) override {
    output_cb_ = output_cb;
    std::move(init_cb).Run(media::DecoderStatus::Codes::kOk);
  }

  void Decode(scoped_refptr<media::DecoderBuffer> buffer,
              DecodeCB done_cb) override {
    DCHECK(output_cb_);

    std::move(done_cb).Run(media::DecoderStatus::Codes::kOk);

    if (!buffer->end_of_stream()) {
      output_cb_.Run(MakeAudioBuffer(kSampleFormat, kChannelLayout, kChannels,
                                     kSamplesPerSecond, 1.0f, 0.0f,
                                     kInputFramesChunk, buffer->timestamp()));
    }
  }

  void Reset(base::OnceClosure closure) override { std::move(closure).Run(); }

 private:
  OutputCB output_cb_;
};

class FakeMojoMediaClient : public media::MojoMediaClient {
 public:
  FakeMojoMediaClient() = default;
  FakeMojoMediaClient(const FakeMojoMediaClient&) = delete;
  FakeMojoMediaClient& operator=(const FakeMojoMediaClient&) = delete;

  std::unique_ptr<media::AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<media::MediaLog> media_log) override {
    return std::make_unique<FakeAudioDecoder>();
  }
};

// Other end of remote InterfaceFactory requested by AudioDecoderBroker. Used
// to create our (fake) media::mojom::AudioDecoder.
class FakeInterfaceFactory : public media::mojom::InterfaceFactory {
 public:
  FakeInterfaceFactory() = default;
  ~FakeInterfaceFactory() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<media::mojom::InterfaceFactory>(
        std::move(handle)));
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &FakeInterfaceFactory::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // Implement this one interface from mojom::InterfaceFactory. Using the real
  // MojoAudioDecoderService allows us to reuse buffer conversion code. The
  // FakeMojoMediaClient will create a FakeGpuAudioDecoder.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) override {
    audio_decoder_receivers_.Add(
        std::make_unique<media::MojoAudioDecoderService>(
            &mojo_media_client_, &cdm_service_context_,
            base::SingleThreadTaskRunner::GetCurrentDefault()),
        std::move(receiver));
  }
  void CreateAudioEncoder(
      mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) override {}

  // Stub out other mojom::InterfaceFactory interfaces.
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder) override {}
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
          video_decoder) override {}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#endif
#if BUILDFLAG(IS_ANDROID)
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) override {}
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {}
#endif  // BUILDFLAG(IS_ANDROID)
  void CreateCdm(const media::CdmConfig& cdm_config,
                 CreateCdmCallback callback) override {
    std::move(callback).Run(mojo::NullRemote(), nullptr,
                            media::CreateCdmStatus::kCdmNotSupported);
  }
#if BUILDFLAG(IS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<
          ::media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote) override {}
#endif  // BUILDFLAG(IS_WIN)

 private:
  FakeMojoMediaClient mojo_media_client_;
  media::MojoCdmServiceContext cdm_service_context_;
  mojo::Receiver<media::mojom::InterfaceFactory> receiver_{this};
  mojo::UniqueReceiverSet<media::mojom::AudioDecoder> audio_decoder_receivers_;
};

}  // namespace

class AudioDecoderBrokerTest : public testing::Test {
 public:
  AudioDecoderBrokerTest() = default;
  ~AudioDecoderBrokerTest() override = default;

  void OnInitWithClosure(base::RepeatingClosure done_cb,
                         media::DecoderStatus status) {
    OnInit(status);
    done_cb.Run();
  }
  void OnDecodeDoneWithClosure(base::RepeatingClosure done_cb,
                               media::DecoderStatus status) {
    OnDecodeDone(std::move(status));
    done_cb.Run();
  }

  void OnResetDoneWithClosure(base::RepeatingClosure done_cb) {
    OnResetDone();
    done_cb.Run();
  }

  MOCK_METHOD1(OnInit, void(media::DecoderStatus status));
  MOCK_METHOD1(OnDecodeDone, void(media::DecoderStatus));
  MOCK_METHOD0(OnResetDone, void());

  void OnOutput(scoped_refptr<media::AudioBuffer> buffer) {
    output_buffers_.push_back(std::move(buffer));
  }

  void SetupMojo(ExecutionContext& execution_context) {
    // Register FakeInterfaceFactory as impl for media::mojom::InterfaceFactory
    // required by MojoAudioDecoder. The factory will vend FakeGpuAudioDecoders
    // that simulate gpu-accelerated decode.
    interface_factory_ = std::make_unique<FakeInterfaceFactory>();
    EXPECT_TRUE(
        Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
            media::mojom::InterfaceFactory::Name_,
            WTF::BindRepeating(&FakeInterfaceFactory::BindRequest,
                               base::Unretained(interface_factory_.get()))));
  }

  void ConstructDecoder(ExecutionContext& execution_context) {
    decoder_broker_ = std::make_unique<AudioDecoderBroker>(&null_media_log_,
                                                           execution_context);
  }

  void InitializeDecoder(media::AudioDecoderConfig config) {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnInit(media::SameStatusCode(media::DecoderStatus(
                           media::DecoderStatus::Codes::kOk))));
    decoder_broker_->Initialize(
        config, nullptr /* cdm_context */,
        WTF::BindOnce(&AudioDecoderBrokerTest::OnInitWithClosure,
                      WTF::Unretained(this), run_loop.QuitClosure()),
        WTF::BindRepeating(&AudioDecoderBrokerTest::OnOutput,
                           WTF::Unretained(this)),
        media::WaitingCB());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void DecodeBuffer(scoped_refptr<media::DecoderBuffer> buffer,
                    media::DecoderStatus::Codes expected_status =
                        media::DecoderStatus::Codes::kOk) {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnDecodeDone(HasStatusCode(expected_status)));
    decoder_broker_->Decode(
        buffer, WTF::BindOnce(&AudioDecoderBrokerTest::OnDecodeDoneWithClosure,
                              WTF::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

  void ResetDecoder() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnResetDone());
    decoder_broker_->Reset(
        WTF::BindOnce(&AudioDecoderBrokerTest::OnResetDoneWithClosure,
                      WTF::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

  media::AudioDecoderType GetDecoderType() {
    return decoder_broker_->GetDecoderType();
  }

  bool IsPlatformDecoder() { return decoder_broker_->IsPlatformDecoder(); }
  bool SupportsDecryption() { return decoder_broker_->SupportsDecryption(); }

 protected:
  test::TaskEnvironment task_environment_;
  media::NullMediaLog null_media_log_;
  std::unique_ptr<AudioDecoderBroker> decoder_broker_;
  std::vector<scoped_refptr<media::AudioBuffer>> output_buffers_;
  std::unique_ptr<FakeInterfaceFactory> interface_factory_;
};

TEST_F(AudioDecoderBrokerTest, Decode_Uninitialized) {
  V8TestingScope v8_scope;

  ConstructDecoder(*v8_scope.GetExecutionContext());
  EXPECT_EQ(GetDecoderType(), media::AudioDecoderType::kBroker);

  // No call to Initialize. Other APIs should fail gracefully.

  DecodeBuffer(media::ReadTestDataFile("vorbis-packet-0"),
               media::DecoderStatus::Codes::kNotInitialized);
  DecodeBuffer(media::DecoderBuffer::CreateEOSBuffer(),
               media::DecoderStatus::Codes::kNotInitialized);
  ASSERT_EQ(0U, output_buffers_.size());

  ResetDecoder();
}

media::AudioDecoderConfig MakeVorbisConfig() {
  std::string extradata_name = "vorbis-extradata";
  base::FilePath extradata_path = media::GetTestDataFilePath(extradata_name);
  int64_t tmp = 0;
  CHECK(base::GetFileSize(extradata_path, &tmp))
      << "Failed to get file size for '" << extradata_name << "'";
  int file_size = base::checked_cast<int>(tmp);
  std::vector<uint8_t> extradata(file_size);
  CHECK_EQ(file_size,
           base::ReadFile(extradata_path,
                          reinterpret_cast<char*>(&extradata[0]), file_size))
      << "Failed to read '" << extradata_name << "'";

  return media::AudioDecoderConfig(kCodec, kSampleFormat, kChannelLayout,
                                   kSamplesPerSecond, std::move(extradata),
                                   media::EncryptionScheme::kUnencrypted);
}

TEST_F(AudioDecoderBrokerTest, Decode_NoMojoDecoder) {
  V8TestingScope v8_scope;

  ConstructDecoder(*v8_scope.GetExecutionContext());
  EXPECT_EQ(GetDecoderType(), media::AudioDecoderType::kBroker);

  InitializeDecoder(MakeVorbisConfig());
  EXPECT_EQ(GetDecoderType(), media::AudioDecoderType::kFFmpeg);

  DecodeBuffer(
      media::ReadTestDataFile("vorbis-packet-0", base::Milliseconds(0)));
  DecodeBuffer(
      media::ReadTestDataFile("vorbis-packet-1", base::Milliseconds(1)));
  DecodeBuffer(
      media::ReadTestDataFile("vorbis-packet-2", base::Milliseconds(2)));
  DecodeBuffer(media::DecoderBuffer::CreateEOSBuffer());
  // 2, not 3, because the first frame doesn't generate an output.
  ASSERT_EQ(2U, output_buffers_.size());

  ResetDecoder();

  DecodeBuffer(
      media::ReadTestDataFile("vorbis-packet-0", base::Milliseconds(0)));
  DecodeBuffer(
      media::ReadTestDataFile("vorbis-packet-1", base::Milliseconds(1)));
  DecodeBuffer(
      media::ReadTestDataFile("vorbis-packet-2", base::Milliseconds(2)));
  DecodeBuffer(media::DecoderBuffer::CreateEOSBuffer());
  // 2 more than last time.
  ASSERT_EQ(4U, output_buffers_.size());

  ResetDecoder();
}

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
TEST_F(AudioDecoderBrokerTest, Decode_WithMojoDecoder) {
  V8TestingScope v8_scope;
  ExecutionContext* execution_context = v8_scope.GetExecutionContext();

  SetupMojo(*execution_context);
  ConstructDecoder(*execution_context);
  EXPECT_EQ(GetDecoderType(), media::AudioDecoderType::kBroker);
  EXPECT_FALSE(IsPlatformDecoder());
  EXPECT_FALSE(SupportsDecryption());

  // Use an MpegH config to prevent FFmpeg from being selected.
  InitializeDecoder(media::AudioDecoderConfig(
      media::AudioCodec::kMpegHAudio, kSampleFormat, kChannelLayout,
      kSamplesPerSecond, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted));
  EXPECT_EQ(GetDecoderType(), media::AudioDecoderType::kTesting);

  // Using vorbis buffer here because its easy and the fake decoder generates
  // output regardless of the input details.
  DecodeBuffer(media::ReadTestDataFile("vorbis-packet-0"));
  DecodeBuffer(media::DecoderBuffer::CreateEOSBuffer());
  // Our fake decoder immediately generates output for any input.
  ASSERT_EQ(1U, output_buffers_.size());

  // True for MojoAudioDecoder.
  EXPECT_TRUE(IsPlatformDecoder());
  // True for for MojoVideoDecoder on Android, but WebCodecs doesn't do
  // decryption, so this is hard-coded to false.
  EXPECT_FALSE(SupportsDecryption());

  ResetDecoder();
}
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
}  // namespace blink
