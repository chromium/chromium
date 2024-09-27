// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_recorder.h"

#include <stdint.h>

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/mock_media_log.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/services/mojo_audio_encoder_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_mojo_encoder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/weak_cell.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/opus/src/include/opus.h"

#if BUILDFLAG(IS_WIN)
#include <objbase.h>

#include "media/gpu/windows/mf_audio_encoder.h"
#define HAS_AAC_ENCODER 1
#endif  //  BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC) && BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/mac/audio_toolbox_audio_encoder.h"
#define HAS_AAC_ENCODER 1
#endif  // BUILDFLAG(IS_MAC) && BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_FFMPEG) && BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/ffmpeg_audio_decoder.h"
#define HAS_AAC_DECODER 1
#endif  // BUILDFLAG(ENABLE_FFMPEG) && BUILDFLAG(USE_PROPRIETARY_CODECS)

using base::TimeTicks;
using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::Invoke;

namespace {

constexpr int kExpectedNumOutputs = 5;
constexpr int kDefaultSampleRate = 48000;

// The duration of the frames that make up the test data we will provide to the
// `AudioTrackRecorder`.
constexpr int kMediaStreamAudioTrackBufferDurationMs = 10;

// This is the preferred opus buffer duration (60 ms), which corresponds to a
// value of 2880 frames per buffer at a sample rate of 48 khz.
constexpr int kOpusBufferDurationMs = 60;

// AAC puts 1024 PCM samples into each AAC frame, which corresponds to a
// duration of 21 and 1/3 milliseconds at a sample rate of 48 khz.
constexpr int kAacFramesPerBuffer = 1024;

int FramesPerInputBuffer(int sample_rate) {
  return kMediaStreamAudioTrackBufferDurationMs * sample_rate /
         base::Time::kMillisecondsPerSecond;
}

#if HAS_AAC_ENCODER
// Other end of remote `InterfaceFactory` requested by `AudioTrackMojoEncoder`.
// Used to create real `media::mojom::AudioEncoders`.
class TestInterfaceFactory : public media::mojom::InterfaceFactory {
 public:
  TestInterfaceFactory() = default;
  ~TestInterfaceFactory() override {
#if BUILDFLAG(IS_WIN)
    ::CoUninitialize();
#endif  // BUILDFLAG(IS_WIN)
  }

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<media::mojom::InterfaceFactory>(
        std::move(handle)));

    // Each `AudioTrackMojoEncoder` instance will try to open a connection to
    // this factory, so we must clean up after each one is destroyed.
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &TestInterfaceFactory::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // Implement this one interface from mojom::InterfaceFactory.
  void CreateAudioEncoder(
      mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) override {
    // While we'd like to use the real `GpuMojoMediaFactory` here, it requires
    // quite a bit more of scaffolding to setup and isn't really needed.
#if BUILDFLAG(IS_MAC)
    auto platform_audio_encoder =
        std::make_unique<media::AudioToolboxAudioEncoder>();
#elif BUILDFLAG(IS_WIN)
    HRESULT hr = ::CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    DCHECK(SUCCEEDED(hr));
    auto platform_audio_encoder = std::make_unique<media::MFAudioEncoder>(
        blink::scheduler::GetSequencedTaskRunnerForTesting());
#else
#error "Unknown platform encoder."
#endif
    audio_encoder_receivers_.Add(
        std::make_unique<media::MojoAudioEncoderService>(
            std::move(platform_audio_encoder)),
        std::move(receiver));
  }

  // Stub out other `mojom::InterfaceFactory` interfaces.
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder) override {}
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) override {}
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
  mojo::Receiver<media::mojom::InterfaceFactory> receiver_{this};
  mojo::UniqueReceiverSet<media::mojom::AudioEncoder> audio_encoder_receivers_;
};
#endif  // HAS_AAC_ENCODER

}  // namespace

namespace blink {

struct ATRTestParams {
  const media::ChannelLayoutConfig channel_layout;
  const int sample_rate;
  const AudioTrackRecorder::CodecId codec;
  const AudioTrackRecorder::BitrateMode bitrate_mode;
};

const ATRTestParams kATRTestParams[] = {
    // Equivalent to default settings:
    {media::ChannelLayoutConfig::Stereo(),        /* channel layout */
     kDefaultSampleRate,                          /* sample rate */
     AudioTrackRecorder::CodecId::kOpus,          /* codec for encoding */
     AudioTrackRecorder::BitrateMode::kVariable}, /* constant/variable rate */

    // Change to mono:
    {media::ChannelLayoutConfig::Mono(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},

    // Different sampling rate as well:
    {media::ChannelLayoutConfig::Mono(), 24000,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig::Stereo(), 8000,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},

    // Using a non-default Opus sampling rate (48, 24, 16, 12, or 8 kHz).
    {media::ChannelLayoutConfig::Mono(), 22050,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig::Stereo(), 44100,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig::Stereo(), 96000,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kVariable},

    // Use Opus in constant bitrate mode:
    {media::ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kOpus,
     AudioTrackRecorder::BitrateMode::kConstant},

    // Use PCM encoder.
    {media::ChannelLayoutConfig::Mono(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kPcm,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kPcm,
     AudioTrackRecorder::BitrateMode::kVariable},

#if HAS_AAC_ENCODER
    {media::ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kAac,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig::Mono(), kDefaultSampleRate,
     AudioTrackRecorder::CodecId::kAac,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig::Stereo(), 44100,
     AudioTrackRecorder::CodecId::kAac,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig::Mono(), 44100,
     AudioTrackRecorder::CodecId::kAac,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig(media::CHANNEL_LAYOUT_5_1_BACK, 6), 44100,
     AudioTrackRecorder::CodecId::kAac,
     AudioTrackRecorder::BitrateMode::kVariable},
    {media::ChannelLayoutConfig(media::CHANNEL_LAYOUT_5_1_BACK, 6),
     kDefaultSampleRate, AudioTrackRecorder::CodecId::kAac,
     AudioTrackRecorder::BitrateMode::kVariable},
#endif  // HAS_AAC_ENCODER
};

std::string ParamsToString(
    const ::testing::TestParamInfo<ATRTestParams>& info) {
  std::stringstream test_suffix;
  switch (info.param.codec) {
    case AudioTrackRecorder::CodecId::kPcm:
      test_suffix << "Pcm";
      break;
    case AudioTrackRecorder::CodecId::kOpus:
      test_suffix << "Opus";
      break;
    case AudioTrackRecorder::CodecId::kAac:
      test_suffix << "Aac";
      break;
    default:
      break;
  }

  if (info.param.channel_layout.channel_layout() ==
      media::ChannelLayout::CHANNEL_LAYOUT_MONO) {
    test_suffix << "Mono";
  } else if (info.param.channel_layout.channel_layout() ==
             media::ChannelLayout::CHANNEL_LAYOUT_STEREO) {
    test_suffix << "Stereo";
  } else if (info.param.channel_layout.channel_layout() ==
             media::ChannelLayout::CHANNEL_LAYOUT_5_1_BACK) {
    test_suffix << "Surround";
  }

  test_suffix << info.param.sample_rate << "hz";

  if (info.param.bitrate_mode == AudioTrackRecorder::BitrateMode::kVariable)
    test_suffix << "VBR";
  else
    test_suffix << "CBR";

  return test_suffix.str();
}

class MockAudioTrackRecorderCallbackInterface
    : public GarbageCollected<MockAudioTrackRecorderCallbackInterface>,
      public AudioTrackRecorder::CallbackInterface {
 public:
  virtual ~MockAudioTrackRecorderCallbackInterface() = default;
  MOCK_METHOD(
      void,
      OnEncodedAudio,
      (const media::AudioParameters& params,
       scoped_refptr<media::DecoderBuffer> encoded_data,
       std::optional<media::AudioEncoder::CodecDescription> codec_description,
       base::TimeTicks capture_time),
      (override));
  MOCK_METHOD(void,
              OnAudioEncodingError,
              (media::EncoderStatus status),
              (override));
  MOCK_METHOD(void, OnSourceReadyStateChanged, (), (override));
  void Trace(Visitor* v) const override { v->Trace(weak_factory_); }
  WeakCell<AudioTrackRecorder::CallbackInterface>* GetWeakCell() {
    return weak_factory_.GetWeakCell();
  }

 private:
  WeakCellFactory<AudioTrackRecorder::CallbackInterface> weak_factory_{this};
};

class AudioTrackRecorderTest : public testing::TestWithParam<ATRTestParams> {
 public:
  // Initialize `first_params_` based on test parameters, and `second_params_`
  // to always be different than `first_params_`.
  AudioTrackRecorderTest()
      : mock_callback_interface_(
            MakeGarbageCollected<MockAudioTrackRecorderCallbackInterface>()),
        codec_(GetParam().codec),
        first_params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      GetParam().channel_layout,
                      GetParam().sample_rate,
                      FramesPerInputBuffer(GetParam().sample_rate)),
        second_params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                       GetParam().channel_layout.channel_layout() ==
                               media::ChannelLayout::CHANNEL_LAYOUT_MONO
                           ? media::ChannelLayoutConfig::Stereo()
                           : media::ChannelLayoutConfig::Mono(),
                       kDefaultSampleRate,
                       FramesPerInputBuffer(kDefaultSampleRate)),
        first_source_(first_params_.channels(),
                      /*freq=*/440,
                      first_params_.sample_rate()),
        second_source_(second_params_.channels(),
                       /*freq=*/440,
                       second_params_.sample_rate()) {
    CHECK(mock_callback_interface_);
  }

  AudioTrackRecorderTest(const AudioTrackRecorderTest&) = delete;
  AudioTrackRecorderTest& operator=(const AudioTrackRecorderTest&) = delete;

  ~AudioTrackRecorderTest() override {
    media_stream_component_ = nullptr;
    mock_callback_interface_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
    audio_track_recorder_.reset();
    // Let the message loop run to finish destroying the recorder properly.
    base::RunLoop().RunUntilIdle();

    // Shutdown the decoder last in case any final callbacks come through.
    ShutdownDecoder();
  }

  void SetUp() override {
    CalculateBufferInformation();
    PrepareTrack();
    InitializeRecorder();
    EXPECT_CALL(*mock_callback_interface_, OnEncodedAudio)
        .WillRepeatedly(
            Invoke([this](const media::AudioParameters& params,
                          scoped_refptr<media::DecoderBuffer> encoded_data,
                          std::optional<media::AudioEncoder::CodecDescription>
                              codec_description,
                          base::TimeTicks capture_time) {
              OnEncodedAudio(params, encoded_data, std::move(codec_description),
                             capture_time);
            }));
  }

  void TearDown() override {
    // We must unregister the current TestInterfaceFactory so the next test can
    // register a new one. Otherwise, it will be run on a new sequence and
    // DCHECK.
    PostCrossThreadTask(
        *encoder_task_runner_.get(), FROM_HERE, CrossThreadBindOnce([] {
          bool result = Platform::Current()
                            ->GetBrowserInterfaceBroker()
                            ->SetBinderForTesting(
                                media::mojom::InterfaceFactory::Name_, {});
          DCHECK(result);
        }));
  }

  void InitializeRecorder() {
    // We create the encoder sequence and provide it to the recorder so we can
    // hold onto a reference to the task runner. This allows us to post tasks to
    // the sequence and apply the necessary overrides, without friending the
    // class.
    encoder_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner({});
    audio_track_recorder_ = std::make_unique<AudioTrackRecorder>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), codec_,
        media_stream_component_, mock_callback_interface_->GetWeakCell(),
        0u /* bits_per_second */, GetParam().bitrate_mode,
        encoder_task_runner_);

#if HAS_AAC_ENCODER
    if (codec_ == AudioTrackRecorder::CodecId::kAac) {
      PostCrossThreadTask(
          *encoder_task_runner_.get(), FROM_HERE, CrossThreadBindOnce([] {
            auto interface_factory = std::make_unique<TestInterfaceFactory>();
            bool result =
                Platform::Current()
                    ->GetBrowserInterfaceBroker()
                    ->SetBinderForTesting(
                        media::mojom::InterfaceFactory::Name_,
                        WTF::BindRepeating(
                            &TestInterfaceFactory::BindRequest,
                            base::Owned(std::move(interface_factory))));
            CHECK(result);
          }));
    }
#endif  // HAS_AAC_ENCODER

    // Give ATR initial audio parameters.
    SetRecorderFormat(first_params_);
  }

  void SetRecorderFormat(const media::AudioParameters& params) {
    audio_track_recorder_->OnSetFormat(params);
    InitializeDecoder(codec_, params);
    first_input_ = true;
    excess_input_ = 0;
  }

  void InitializeDecoder(const AudioTrackRecorder::CodecId codec,
                         const media::AudioParameters& params) {
    ShutdownDecoder();
    if (codec == AudioTrackRecorder::CodecId::kOpus) {
      int error;
      opus_decoder_ =
          opus_decoder_create(kDefaultSampleRate, params.channels(), &error);
      EXPECT_TRUE(error == OPUS_OK && opus_decoder_);

      opus_buffer_.reset(new float[opus_buffer_size_]);
    } else if (codec == AudioTrackRecorder::CodecId::kAac) {
#if HAS_AAC_DECODER
      InitializeAacDecoder(params.channels(), params.sample_rate());
#endif  // HAS_AAC_DECODER
    }
  }

  void ShutdownDecoder() {
    if (opus_decoder_) {
      opus_decoder_destroy(opus_decoder_);
      opus_decoder_ = nullptr;
    }
  }

  void CalculateBufferInformation() {
    switch (codec_) {
      case AudioTrackRecorder::CodecId::kPcm:
        frames_per_buffer_ = FramesPerInputBuffer(GetParam().sample_rate);
        break;
      case AudioTrackRecorder::CodecId::kOpus:
        // According to documentation in third_party/opus/src/include/opus.h,
        // we must provide enough space in |buffer_| to contain 120ms of
        // samples.
        // Opus always resamples the input to be `kDefaultSampleRate`, so we use
        // that instead of the params.
        opus_buffer_size_ =
            120 * kDefaultSampleRate *
            std::max(first_params_.channels(), second_params_.channels()) /
            base::Time::kMillisecondsPerSecond;
        frames_per_buffer_ = kOpusBufferDurationMs * kDefaultSampleRate /
                             base::Time::kMillisecondsPerSecond;
        break;
      case AudioTrackRecorder::CodecId::kAac:
        frames_per_buffer_ = kAacFramesPerBuffer;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  void GenerateAndRecordAudio(bool use_first_source) {
    int sample_rate = use_first_source ? first_params_.sample_rate()
                                       : second_params_.sample_rate();
    int num_inputs = GetNumInputsNeeded(kExpectedNumOutputs, sample_rate);
    for (int i = 0; i < num_inputs; ++i) {
      if (use_first_source) {
        audio_track_recorder_->OnData(*GetFirstSourceAudioBus(),
                                      base::TimeTicks::Now());
      } else {
        audio_track_recorder_->OnData(*GetSecondSourceAudioBus(),
                                      base::TimeTicks::Now());
      }
    }

    // If we're paused the input is discarded so don't unset `first_input_`.
    if (!paused_)
      first_input_ = false;
  }

  int GetNumInputsNeeded(int desired_num_outputs, int sample_rate) {
    if (codec_ == AudioTrackRecorder::CodecId::kPcm)
      return desired_num_outputs;

#if HAS_AAC_ENCODER && BUILDFLAG(IS_WIN)
    // The AAC encoder on Windows buffers two output frames. So, we need
    // enough input to fill these buffers before we will receive output, if we
    // haven't provided any other input.
    if (first_input_ && codec_ == AudioTrackRecorder::CodecId::kAac)
      desired_num_outputs += 2;
#endif  // HAS_AAC_ENCODER

    int inputs_per_output;
    if (codec_ == AudioTrackRecorder::CodecId::kOpus) {
      // Opus resamples the input to use `kDefaultSampleRate`
      inputs_per_output =
          frames_per_buffer_ / FramesPerInputBuffer(kDefaultSampleRate);
    } else {
      inputs_per_output =
          frames_per_buffer_ / FramesPerInputBuffer(sample_rate);
    }

    int num_inputs_needed = desired_num_outputs * inputs_per_output;
    if (codec_ == AudioTrackRecorder::CodecId::kOpus) {
      if (frames_per_buffer_ % FramesPerInputBuffer(sample_rate))
        return ++num_inputs_needed;
    }

    // If the number of input frames doesn't evenly divide the number of frames
    // in an output buffer, we will have underestimated `num_inputs_needed` and
    // may need to provide additional input buffers.
    //
    // Say `FramesPerInputBuffer` is 30, and `frames_per_buffer_` is 100.
    // `num_inputs_needed` will truncate to 3, and we will be short 10 frames
    // and will not get the `desired_num_outputs`. So, `num_inputs_needed`
    // should be increased. If `desired_num_outputs` is 5, we need to provide 2
    // additional inputs, since we will be short 50 input frames.
    //
    // Additionally, this leads to an excess of 10 frames of input provided,
    // which we need to track so we don't provide too much input next time.
    int additional_input_frames = frames_per_buffer_ %
                                  FramesPerInputBuffer(sample_rate) *
                                  desired_num_outputs;

    // Apply the excess input.
    additional_input_frames -= excess_input_;

    // If we're paused, the input won't actually make it to the encoder, so
    // don't modify `excess_input_`.
    if (!paused_)
      excess_input_ = 0;

    // If the excess input did not cover it, we will need additional inputs.
    while (additional_input_frames > 0) {
      ++num_inputs_needed;
      additional_input_frames -= FramesPerInputBuffer(sample_rate);
    }

    // If `additional_input_frames` is negative, we provided excess input equal
    // to the magnitude.
    if (!paused_ && additional_input_frames < 0)
      excess_input_ = -additional_input_frames;

    return num_inputs_needed;
  }

  std::unique_ptr<media::AudioBus> GetFirstSourceAudioBus() {
    std::unique_ptr<media::AudioBus> bus(media::AudioBus::Create(
        first_params_.channels(),
        FramesPerInputBuffer(first_params_.sample_rate())));
    first_source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {},
                             bus.get());

    // Save the samples that we read into the first_source_cache_ if we're using
    // the PCM encoder, so we can verify the output data later. Do not save it
    // if the recorder is paused.
    if (codec_ == AudioTrackRecorder::CodecId::kPcm && !paused_) {
      std::unique_ptr<media::AudioBus> cache_bus(
          media::AudioBus::Create(bus->channels(), bus->frames()));
      bus->CopyTo(cache_bus.get());

      int current_size = first_source_cache_.size();
      first_source_cache_.resize(current_size +
                                 cache_bus->frames() * cache_bus->channels());
      cache_bus->ToInterleaved<media::Float32SampleTypeTraits>(
          cache_bus->frames(), &first_source_cache_[current_size]);
    }

    return bus;
  }

  std::unique_ptr<media::AudioBus> GetSecondSourceAudioBus() {
    std::unique_ptr<media::AudioBus> bus(media::AudioBus::Create(
        second_params_.channels(),
        FramesPerInputBuffer(second_params_.sample_rate())));
    second_source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), {},
                              bus.get());
    return bus;
  }

  void ExpectOutputsAndRunClosure(base::OnceClosure closure) {
#if HAS_AAC_ENCODER
    if (GetParam().codec == AudioTrackRecorder::CodecId::kAac) {
      EXPECT_CALL(*this, DoOnEncodedAudio)
          .Times(kExpectedNumOutputs - 1)
          .InSequence(s_);
#if HAS_AAC_DECODER
      EXPECT_CALL(*this, DoOnEncodedAudio).InSequence(s_);

      // The `FFmpegAudioDecoder` will call `DecodeOutputCb` and then
      // `DecodeCb` for each encoded buffer we provide. These need a
      // separate sequence since they are asynchronous and we can't predict
      // their execution order in relation to `DoOnEncodeAudio`. They must
      // be run in sequence so the closure is run once these expectations
      // are saturated.
      for (int i = 0; i < kExpectedNumOutputs - 1; i++) {
        EXPECT_CALL(*this, DecodeOutputCb).InSequence(s2_);
        EXPECT_CALL(*this, DecodeCb).InSequence(s2_);
      }

      // Once we have the minimum number of outputs, we must run the closure.
      EXPECT_CALL(*this, DecodeOutputCb).InSequence(s2_);
      EXPECT_CALL(*this, DecodeCb)
          .InSequence(s2_)
          .WillOnce(RunOnceClosure(std::move(closure)));
#else
      // If we don't have a decoder, we must run the closure after the final
      // call to `DoOnEncodedAudio`.
      EXPECT_CALL(*this, DoOnEncodedAudio)
          .InSequence(s_)
          .WillOnce(RunOnceClosure(std::move(closure)));
#endif  // HAS_AAC_DECODER
      return;
    }
#endif  // HAS_AAC_ENCODER

    EXPECT_CALL(*this, DoOnEncodedAudio)
        .Times(kExpectedNumOutputs - 1)
        .InSequence(s_);
    EXPECT_CALL(*this, DoOnEncodedAudio)
        .InSequence(s_)
        .WillOnce(RunOnceClosure(std::move(closure)));
  }

  MOCK_METHOD(void,
              DoOnEncodedAudio,
              (const media::AudioParameters& params,
               scoped_refptr<media::DecoderBuffer> encoded_data,
               base::TimeTicks timestamp));

  void OnEncodedAudio(
      const media::AudioParameters& params,
      scoped_refptr<media::DecoderBuffer> encoded_data,
      std::optional<media::AudioEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) {
    EXPECT_TRUE(!encoded_data->empty());

    switch (codec_) {
      case AudioTrackRecorder::CodecId::kOpus:
        ValidateOpusData(encoded_data);
        break;
      case AudioTrackRecorder::CodecId::kPcm:
        ValidatePcmData(encoded_data);
        break;
      case AudioTrackRecorder::CodecId::kAac:
#if HAS_AAC_DECODER
        ValidateAacData(encoded_data);
#endif  // HAS_AAC_DECODER
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    DoOnEncodedAudio(params, std::move(encoded_data), timestamp);
  }

  void ValidateOpusData(scoped_refptr<media::DecoderBuffer> encoded_data) {
    // Decode |encoded_data| and check we get the expected number of frames
    // per buffer.
    ASSERT_GE(static_cast<size_t>(opus_buffer_size_), encoded_data->size());
    EXPECT_EQ(kDefaultSampleRate * kOpusBufferDurationMs / 1000,
              opus_decode_float(opus_decoder_, encoded_data->data(),
                                static_cast<wtf_size_t>(encoded_data->size()),
                                opus_buffer_.get(), opus_buffer_size_, 0));
  }

  void ValidatePcmData(scoped_refptr<media::DecoderBuffer> encoded_data) {
    // Manually confirm that we're getting the same data out as what we
    // generated from the sine wave.
    for (size_t b = 0; b + 3 < encoded_data->size() &&
                       first_source_cache_pos_ < first_source_cache_.size();
         b += sizeof(first_source_cache_[0]), ++first_source_cache_pos_) {
      float sample;
      memcpy(&sample, encoded_data->AsSpan().subspan(b).data(), 4);
      ASSERT_FLOAT_EQ(sample, first_source_cache_[first_source_cache_pos_])
          << "(Sample " << first_source_cache_pos_ << ")";
    }
  }

  test::TaskEnvironment task_environment_;

#if HAS_AAC_DECODER
  void ValidateAacData(scoped_refptr<media::DecoderBuffer> encoded_data) {
    // `ExpectOutputsAndRunClosure` sets up `EXPECT_CALL`s for `DecodeCB` and
    // `DecodeOutputCb`, so we can be sure that these will run and the decoded
    // output is validated.
    media::AudioDecoder::DecodeCB decode_cb =
        WTF::BindOnce(&AudioTrackRecorderTest::OnDecode, WTF::Unretained(this));
    aac_decoder_->Decode(encoded_data, std::move(decode_cb));
  }

  void InitializeAacDecoder(int channels, int sample_rate) {
    aac_decoder_ = std::make_unique<media::FFmpegAudioDecoder>(
        scheduler::GetSequencedTaskRunnerForTesting(), &media_log_);
    media::ChannelLayout channel_layout = media::CHANNEL_LAYOUT_NONE;
    switch (channels) {
      case 1:
        channel_layout = media::ChannelLayout::CHANNEL_LAYOUT_MONO;
        break;
      case 2:
        channel_layout = media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
        break;
      case 6:
        channel_layout = media::ChannelLayout::CHANNEL_LAYOUT_5_1_BACK;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    media::AudioDecoderConfig config(media::AudioCodec::kAAC,
                                     media::SampleFormat::kSampleFormatS16,
                                     channel_layout, sample_rate,
                                     /*extra_data=*/std::vector<uint8_t>(),
                                     media::EncryptionScheme::kUnencrypted);
    EXPECT_CALL(*this, InitCb);
    media::AudioDecoder::InitCB init_cb =
        WTF::BindOnce(&AudioTrackRecorderTest::OnInit, WTF::Unretained(this));
    media::AudioDecoder::OutputCB output_cb = WTF::BindRepeating(
        &AudioTrackRecorderTest::OnDecodeOutput, WTF::Unretained(this));
    aac_decoder_->Initialize(config, /*cdm_context=*/nullptr,
                             std::move(init_cb), std::move(output_cb),
                             /*waiting_cb=*/base::DoNothing());
  }

  MOCK_METHOD(void, DecodeCb, (media::DecoderStatusTraits::Codes));
  MOCK_METHOD(void, InitCb, (media::DecoderStatusTraits::Codes));
  MOCK_METHOD(void, DecodeOutputCb, (scoped_refptr<media::AudioBuffer>));

  void OnDecode(media::DecoderStatus status) {
    EXPECT_TRUE(status.is_ok());

    DecodeCb(status.code());
  }
  void OnInit(media::DecoderStatus status) {
    EXPECT_TRUE(status.is_ok());
    InitCb(status.code());
  }
  void OnDecodeOutput(scoped_refptr<media::AudioBuffer> decoded_buffer) {
    EXPECT_EQ(decoded_buffer->frame_count(), frames_per_buffer_);
    DecodeOutputCb(decoded_buffer);
  }

  media::MockMediaLog media_log_;
  std::unique_ptr<media::FFmpegAudioDecoder> aac_decoder_;
#endif  // HAS_AAC_DECODER

  ::testing::Sequence s_;
  ::testing::Sequence s2_;
  base::RunLoop run_loop_;

  Persistent<MockAudioTrackRecorderCallbackInterface> mock_callback_interface_;

  // AudioTrackRecorder and MediaStreamComponent for fooling it.
  std::unique_ptr<AudioTrackRecorder> audio_track_recorder_;
  Persistent<MediaStreamComponent> media_stream_component_;

  // The task runner for the encoder thread, so we can post tasks to it.
  scoped_refptr<base::SequencedTaskRunner> encoder_task_runner_;

  // The codec we'll use for compression the audio.
  const AudioTrackRecorder::CodecId codec_;

  // Two different sets of AudioParameters for testing re-init of ATR.
  const media::AudioParameters first_params_;
  const media::AudioParameters second_params_;

  // AudioSources for creating AudioBuses.
  media::SineWaveAudioSource first_source_;
  media::SineWaveAudioSource second_source_;

  // The number of PCM samples in an output buffer.
  int frames_per_buffer_;

  // Some encoders buffer input, so we need to provide extra to fill that
  // buffer, but only once.
  bool first_input_ = true;

  // Sometimes we may provide more input than necessary to the encoder, we track
  // this so we can account for it and not receive unexpected outputs.
  int excess_input_ = 0;

  // Decoder for verifying data was properly encoded.
  raw_ptr<OpusDecoder, DanglingUntriaged> opus_decoder_ = nullptr;
  std::unique_ptr<float[]> opus_buffer_;
  int opus_buffer_size_;

  // Save the data we generate from the first source so that we might compare it
  // later if we happen to be using the PCM encoder.
  Vector<float> first_source_cache_;
  wtf_size_t first_source_cache_pos_ = 0;

  // Track when we are paused so we can correctly expect the encoder's behavior,
  // e.g. tracking `excess_input_` and `first_source_cache_`.
  bool paused_ = false;

 private:
  // Prepares a blink track of a given MediaStreamType and attaches the native
  // track, which can be used to capture audio data and pass it to the producer.
  // Adapted from media::WebRTCLocalAudioSourceProviderTest.
  void PrepareTrack() {
    auto audio_source = std::make_unique<MediaStreamAudioSource>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), true);
    auto* source = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("dummy_source_id"), MediaStreamSource::kTypeAudio,
        String::FromUTF8("dummy_source_name"), false /* remote */,
        std::move(audio_source));
    media_stream_component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        String::FromUTF8("audio_track"), source,
        std::make_unique<MediaStreamAudioTrack>(/*is_local=*/true));
    CHECK(MediaStreamAudioSource::From(source)->ConnectToInitializedTrack(
        media_stream_component_));
  }
};

TEST_P(AudioTrackRecorderTest, RecordAndValidate) {
  ExpectOutputsAndRunClosure(run_loop_.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  run_loop_.Run();
}

TEST_P(AudioTrackRecorderTest, ChangeFormatMidRecording) {
  ExpectOutputsAndRunClosure(run_loop_.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);
  run_loop_.Run();

  // Give ATR new audio parameters.
  SetRecorderFormat(second_params_);

  // Send audio with different params.
  base::RunLoop run_loop2;
  ExpectOutputsAndRunClosure(run_loop2.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/false);

  run_loop2.Run();
}

TEST_P(AudioTrackRecorderTest, ChangeFormatWhilePaused) {
  ExpectOutputsAndRunClosure(run_loop_.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);
  run_loop_.Run();

  // Give ATR new audio parameters.
  audio_track_recorder_->Pause();
  SetRecorderFormat(second_params_);
  audio_track_recorder_->Resume();

  // Send audio with different params.
  base::RunLoop run_loop2;
  ExpectOutputsAndRunClosure(run_loop2.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/false);

  run_loop2.Run();
}

TEST_P(AudioTrackRecorderTest, SameFormat) {
  ExpectOutputsAndRunClosure(run_loop_.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);
  run_loop_.Run();

  // Give ATR the same audio parameters. We don't call `SetRecorderFormat()`
  // because it resets `first_input_` and `excess_input_` which we don't want
  // since setting the same format should not cause the encoder to reinitialize.
  audio_track_recorder_->OnSetFormat(first_params_);

  base::RunLoop run_loop2;
  ExpectOutputsAndRunClosure(run_loop2.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  run_loop2.Run();
}

TEST_P(AudioTrackRecorderTest, PacketSize) {
  // Record the size of the outputs so we can ensure they are the same size
  // if we are using a constant bitrate, or not the same size if we are using
  // variable bitrate.
  std::vector<std::size_t> encodedPacketSizes;
  EXPECT_CALL(*this, DoOnEncodedAudio)
      .Times(kExpectedNumOutputs - 1)
      .InSequence(s_)
      .WillRepeatedly([&encodedPacketSizes](
                          const media::AudioParameters&,
                          scoped_refptr<media::DecoderBuffer> encoded_data,
                          base::TimeTicks) {
        encodedPacketSizes.push_back(encoded_data->size());
      });
  EXPECT_CALL(*this, DoOnEncodedAudio)
      .InSequence(s_)
      .WillOnce(
          testing::DoAll(RunOnceClosure(run_loop_.QuitClosure()),
                         [&encodedPacketSizes](
                             const media::AudioParameters&,
                             scoped_refptr<media::DecoderBuffer> encoded_data,
                             base::TimeTicks) {
                           encodedPacketSizes.push_back(encoded_data->size());
                         }));
  GenerateAndRecordAudio(/*use_first_source=*/true);
  run_loop_.Run();

  ASSERT_GE(encodedPacketSizes.size(), 0ull);
  bool all_packets_same_size = true;
  const size_t& first_size = encodedPacketSizes[0];
  for (size_t i = 1; i < encodedPacketSizes.size(); i++) {
    if (encodedPacketSizes[i] != first_size) {
      all_packets_same_size = false;
      break;
    }
  }

  if (GetParam().bitrate_mode == AudioTrackRecorder::BitrateMode::kConstant)
    EXPECT_TRUE(all_packets_same_size);

  // Even if all packets are the same size, we can't guarantee that the bitrate
  // isn't variable. This test may provide inputs that are all the same size, so
  // the PCM encoder may appear to use a constant bitrate, when it is actually
  // variable.
}

TEST_P(AudioTrackRecorderTest, PauseResume) {
  audio_track_recorder_->Pause();
  paused_ = true;
  EXPECT_CALL(*this, DoOnEncodedAudio).Times(0).InSequence(s_);
  GenerateAndRecordAudio(/*use_first_source=*/true);

  audio_track_recorder_->Resume();
  paused_ = false;
  ExpectOutputsAndRunClosure(run_loop_.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  run_loop_.Run();
}

TEST_P(AudioTrackRecorderTest, PauseMidStream) {
  ExpectOutputsAndRunClosure(base::DoNothing());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  audio_track_recorder_->Pause();
  paused_ = true;
  EXPECT_CALL(*this, DoOnEncodedAudio).Times(0).InSequence(s_);
  GenerateAndRecordAudio(/*use_first_source=*/true);

  audio_track_recorder_->Resume();
  paused_ = false;
  ExpectOutputsAndRunClosure(run_loop_.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  run_loop_.Run();
}

#if BUILDFLAG(IS_MAC)
// This test is flaky on Mac. See https://crbug.com/1370195.
#define MAYBE_PauseTwice DISABLED_PauseTwice
#else
#define MAYBE_PauseTwice PauseTwice
#endif
TEST_P(AudioTrackRecorderTest, MAYBE_PauseTwice) {
  ExpectOutputsAndRunClosure(base::DoNothing());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  audio_track_recorder_->Pause();
  paused_ = true;
  EXPECT_CALL(*this, DoOnEncodedAudio).Times(0).InSequence(s_);
  GenerateAndRecordAudio(/*use_first_source=*/true);

  // A second pause should have no effect.
  audio_track_recorder_->Pause();
  EXPECT_CALL(*this, DoOnEncodedAudio).Times(0).InSequence(s_);
  GenerateAndRecordAudio(/*use_first_source=*/true);

  audio_track_recorder_->Resume();
  paused_ = false;
  ExpectOutputsAndRunClosure(run_loop_.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  run_loop_.Run();
}

TEST_P(AudioTrackRecorderTest, ResumeWithoutPausing) {
  ExpectOutputsAndRunClosure(base::DoNothing());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  // Resume without pausing should have no effect.
  audio_track_recorder_->Resume();
  ExpectOutputsAndRunClosure(run_loop_.QuitClosure());
  GenerateAndRecordAudio(/*use_first_source=*/true);

  run_loop_.Run();
}

INSTANTIATE_TEST_SUITE_P(,
                         AudioTrackRecorderTest,
                         testing::ValuesIn(kATRTestParams),
                         ParamsToString);

}  // namespace blink
