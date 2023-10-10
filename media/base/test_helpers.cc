// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_helpers.h"

#include <stdint.h>

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "ui/gfx/geometry/rect.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

// Utility mock for testing methods expecting Closures and PipelineStatusCBs.
class MockCallback : public base::RefCountedThreadSafe<MockCallback> {
 public:
  MockCallback();

  MockCallback(const MockCallback&) = delete;
  MockCallback& operator=(const MockCallback&) = delete;

  MOCK_METHOD0(Run, void());
  MOCK_METHOD1(RunWithBool, void(bool));
  MOCK_METHOD1(RunWithStatus, void(PipelineStatus));

 protected:
  friend class base::RefCountedThreadSafe<MockCallback>;
  virtual ~MockCallback();
};

MockCallback::MockCallback() = default;
MockCallback::~MockCallback() = default;

base::OnceClosure NewExpectedClosure() {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, Run());
  return base::BindOnce(&MockCallback::Run, WrapRefCounted(callback));
}

base::OnceCallback<void(bool)> NewExpectedBoolCB(bool success) {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, RunWithBool(success));
  return base::BindOnce(&MockCallback::RunWithBool, WrapRefCounted(callback));
}

PipelineStatusCallback NewExpectedStatusCB(PipelineStatus status) {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, RunWithStatus(status));
  return base::BindOnce(&MockCallback::RunWithStatus, WrapRefCounted(callback));
}

WaitableMessageLoopEvent::WaitableMessageLoopEvent()
    : WaitableMessageLoopEvent(TestTimeouts::action_timeout()) {}

WaitableMessageLoopEvent::WaitableMessageLoopEvent(base::TimeDelta timeout)
    : signaled_(false), status_(PIPELINE_OK), timeout_(timeout) {}

WaitableMessageLoopEvent::~WaitableMessageLoopEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::OnceClosure WaitableMessageLoopEvent::GetClosure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindPostTaskToCurrentDefault(
      base::BindOnce(&WaitableMessageLoopEvent::OnCallback,
                     base::Unretained(this), PIPELINE_OK));
}

PipelineStatusCallback WaitableMessageLoopEvent::GetPipelineStatusCB() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindPostTaskToCurrentDefault(base::BindOnce(
      &WaitableMessageLoopEvent::OnCallback, base::Unretained(this)));
}

void WaitableMessageLoopEvent::RunAndWait() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunAndWaitForStatus(PIPELINE_OK);
}

void WaitableMessageLoopEvent::RunAndWaitForStatus(PipelineStatus expected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (signaled_) {
    EXPECT_EQ(expected, status_);
    return;
  }

  run_loop_ = std::make_unique<base::RunLoop>();
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, timeout_,
              base::BindOnce(&WaitableMessageLoopEvent::OnTimeout,
                             base::Unretained(this)));

  run_loop_->Run();
  EXPECT_TRUE(signaled_);
  EXPECT_EQ(expected, status_);
  run_loop_.reset();
}

void WaitableMessageLoopEvent::OnCallback(PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signaled_ = true;
  status_ = status;

  // |run_loop_| may be null if the callback fires before RunAndWaitForStatus().
  if (run_loop_)
    run_loop_->Quit();
}

void WaitableMessageLoopEvent::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ADD_FAILURE() << "Timed out waiting for message loop to quit";
  run_loop_->Quit();
}

static VideoDecoderConfig GetTestConfig(VideoCodec codec,
                                        VideoCodecProfile profile,
                                        const VideoColorSpace& color_space,
                                        VideoRotation rotation,
                                        gfx::Size coded_size,
                                        bool is_encrypted) {
  gfx::Rect visible_rect(coded_size.width(), coded_size.height());
  gfx::Size natural_size = coded_size;

  return VideoDecoderConfig(
      codec, profile, VideoDecoderConfig::AlphaMode::kIsOpaque, color_space,
      VideoTransformation(rotation), coded_size, visible_rect, natural_size,
      EmptyExtraData(),
      is_encrypted ? EncryptionScheme::kCenc : EncryptionScheme::kUnencrypted);
}

static VideoCodecProfile MinProfile(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kUnknown:
    case VideoCodec::kVC1:
    case VideoCodec::kMPEG2:
    case VideoCodec::kMPEG4:
      return VIDEO_CODEC_PROFILE_UNKNOWN;
    case VideoCodec::kH264:
      return H264PROFILE_MIN;
    case VideoCodec::kTheora:
      return THEORAPROFILE_MIN;
    case VideoCodec::kVP8:
      return VP8PROFILE_MIN;
    case VideoCodec::kVP9:
      return VP9PROFILE_MIN;
    case VideoCodec::kHEVC:
      return HEVCPROFILE_MIN;
    case VideoCodec::kDolbyVision:
      return DOLBYVISION_PROFILE0;
    case VideoCodec::kAV1:
      return AV1PROFILE_MIN;
  }
}

static const gfx::Size kNormalSize(320, 240);
static const gfx::Size kLargeSize(640, 480);
static const gfx::Size kExtraLargeSize(15360, 8640);

// static
VideoDecoderConfig TestVideoConfig::Invalid() {
  return GetTestConfig(VideoCodec::kUnknown, VIDEO_CODEC_PROFILE_UNKNOWN,
                       VideoColorSpace::JPEG(), VIDEO_ROTATION_0, kNormalSize,
                       false);
}

// static
VideoDecoderConfig TestVideoConfig::Normal(VideoCodec codec) {
  return GetTestConfig(codec, MinProfile(codec), VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, kNormalSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::NormalWithColorSpace(
    VideoCodec codec,
    const VideoColorSpace& color_space) {
  return GetTestConfig(codec, MinProfile(codec), color_space, VIDEO_ROTATION_0,
                       kNormalSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::NormalH264(VideoCodecProfile config) {
  return GetTestConfig(VideoCodec::kH264, MinProfile(VideoCodec::kH264),
                       VideoColorSpace::JPEG(), VIDEO_ROTATION_0, kNormalSize,
                       false);
}

// static
VideoDecoderConfig TestVideoConfig::NormalCodecProfile(
    VideoCodec codec,
    VideoCodecProfile profile) {
  return GetTestConfig(codec, profile, VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, kNormalSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::NormalEncrypted(VideoCodec codec) {
  return NormalEncrypted(codec, MinProfile(codec));
}

VideoDecoderConfig TestVideoConfig::NormalEncrypted(VideoCodec codec,
                                                    VideoCodecProfile profile) {
  return GetTestConfig(codec, profile, VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, kNormalSize, true);
}

// static
VideoDecoderConfig TestVideoConfig::NormalRotated(VideoRotation rotation) {
  return GetTestConfig(VideoCodec::kVP8, MinProfile(VideoCodec::kVP8),
                       VideoColorSpace::JPEG(), rotation, kNormalSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::Large(VideoCodec codec) {
  return GetTestConfig(codec, MinProfile(codec), VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, kLargeSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::LargeEncrypted(VideoCodec codec) {
  return GetTestConfig(codec, MinProfile(codec), VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, kLargeSize, true);
}

// static
VideoDecoderConfig TestVideoConfig::ExtraLarge(VideoCodec codec) {
  return GetTestConfig(codec, MinProfile(codec), VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, kExtraLargeSize, false);
}

// static
VideoDecoderConfig TestVideoConfig::ExtraLargeEncrypted(VideoCodec codec) {
  return GetTestConfig(codec, MinProfile(codec), VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, kExtraLargeSize, true);
}

// static
VideoDecoderConfig TestVideoConfig::Custom(gfx::Size size, VideoCodec codec) {
  return GetTestConfig(codec, MinProfile(codec), VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, size, false);
}

// static
VideoDecoderConfig TestVideoConfig::CustomEncrypted(gfx::Size size,
                                                    VideoCodec codec) {
  return GetTestConfig(codec, MinProfile(codec), VideoColorSpace::JPEG(),
                       VIDEO_ROTATION_0, size, true);
}

// static
gfx::Size TestVideoConfig::NormalCodedSize() {
  return kNormalSize;
}

// static
gfx::Size TestVideoConfig::LargeCodedSize() {
  return kLargeSize;
}

// static
gfx::Size TestVideoConfig::ExtraLargeCodedSize() {
  return kExtraLargeSize;
}

AudioDecoderConfig TestAudioConfig::Normal() {
  return AudioDecoderConfig(AudioCodec::kVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, NormalSampleRateValue(),
                            EmptyExtraData(), EncryptionScheme::kUnencrypted);
}

AudioDecoderConfig TestAudioConfig::NormalEncrypted() {
  return AudioDecoderConfig(AudioCodec::kVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, NormalSampleRateValue(),
                            EmptyExtraData(), EncryptionScheme::kCenc);
}

AudioDecoderConfig TestAudioConfig::HighSampleRate() {
  return AudioDecoderConfig(AudioCodec::kVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, HighSampleRateValue(),
                            EmptyExtraData(), EncryptionScheme::kUnencrypted);
}

AudioDecoderConfig TestAudioConfig::HighSampleRateEncrypted() {
  return AudioDecoderConfig(AudioCodec::kVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, HighSampleRateValue(),
                            EmptyExtraData(), EncryptionScheme::kCenc);
}

int TestAudioConfig::NormalSampleRateValue() {
  return 44100;
}

int TestAudioConfig::HighSampleRateValue() {
  return 192000;
}

// static
AudioParameters TestAudioParameters::Normal() {
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(), 48000, 2048);
}

template <class T>
scoped_refptr<AudioBuffer> MakeAudioBuffer(SampleFormat format,
                                           ChannelLayout channel_layout,
                                           size_t channel_count,
                                           int sample_rate,
                                           T start,
                                           T increment,
                                           size_t frames,
                                           base::TimeDelta timestamp) {
  const size_t channels = ChannelLayoutToChannelCount(channel_layout);
  scoped_refptr<AudioBuffer> output =
      AudioBuffer::CreateBuffer(format,
                                channel_layout,
                                static_cast<int>(channel_count),
                                sample_rate,
                                static_cast<int>(frames));
  output->set_timestamp(timestamp);

  const bool is_planar = IsPlanar(format);

  // Values in channel 0 will be:
  //   start
  //   start + increment
  //   start + 2 * increment, ...
  // While, values in channel 1 will be:
  //   start + frames * increment
  //   start + (frames + 1) * increment
  //   start + (frames + 2) * increment, ...
  for (size_t ch = 0; ch < channels; ++ch) {
    T* buffer =
        reinterpret_cast<T*>(output->channel_data()[is_planar ? ch : 0]);
    const T v = static_cast<T>(start + ch * frames * increment);
    for (size_t i = 0; i < frames; ++i) {
      buffer[is_planar ? i : ch + i * channels] =
          static_cast<T>(v + i * increment);
    }
  }
  return output;
}

template <>
scoped_refptr<AudioBuffer> MakeAudioBuffer<float>(SampleFormat format,
                                                  ChannelLayout channel_layout,
                                                  size_t channel_count,
                                                  int sample_rate,
                                                  float start,
                                                  float increment,
                                                  size_t frames,
                                                  base::TimeDelta timestamp) {
  const size_t channels = ChannelLayoutToChannelCount(channel_layout);
  scoped_refptr<AudioBuffer> output = AudioBuffer::CreateBuffer(
      format, channel_layout, static_cast<int>(channel_count), sample_rate,
      static_cast<int>(frames));
  output->set_timestamp(timestamp);

  const bool is_planar =
      format == kSampleFormatPlanarS16 || format == kSampleFormatPlanarF32;

  // Values in channel 0 will be:
  //   (start) / max_value
  //   (start + increment) / max_value
  //   (start + 2 * increment) / max_value, ...
  // While, values in channel 1 will be:
  //   (start + frames * increment) / max_value
  //   (start + (frames + 1) * increment) / max_value
  //   (start + (frames + 2) * increment) / max_value, ...
  for (size_t ch = 0; ch < channels; ++ch) {
    float* buffer =
        reinterpret_cast<float*>(output->channel_data()[is_planar ? ch : 0]);
    const float v = static_cast<float>(start + ch * frames * increment);
    for (size_t i = 0; i < frames; ++i) {
      buffer[is_planar ? i : ch + i * channels] =
          static_cast<float>(v + i * increment) /
          std::numeric_limits<uint16_t>::max();
    }
  }
  return output;
}

scoped_refptr<AudioBuffer> MakeBitstreamAudioBuffer(
    SampleFormat format,
    ChannelLayout channel_layout,
    size_t channel_count,
    int sample_rate,
    uint8_t start,
    uint8_t increment,
    size_t frames,
    size_t data_size,
    base::TimeDelta timestamp) {
  scoped_refptr<AudioBuffer> output = AudioBuffer::CreateBitstreamBuffer(
      format, channel_layout, static_cast<int>(channel_count), sample_rate,
      static_cast<int>(frames), data_size);
  output->set_timestamp(timestamp);

  // Values in channel 0 will be:
  //   start
  //   start + increment
  //   start + 2 * increment, ...
  uint8_t* buffer = reinterpret_cast<uint8_t*>(output->channel_data()[0]);
  for (size_t i = 0; i < data_size; ++i) {
    buffer[i] = static_cast<uint8_t>(start + i * increment);
  }

  return output;
}

void VerifyBitstreamAudioBus(AudioBus* bus,
                             size_t data_size,
                             uint8_t start,
                             uint8_t increment) {
  ASSERT_TRUE(bus->is_bitstream_format());

  // Values in channel 0 will be:
  //   start
  //   start + increment
  //   start + 2 * increment, ...
  uint8_t* buffer = reinterpret_cast<uint8_t*>(bus->channel(0));
  for (size_t i = 0; i < data_size; ++i) {
    ASSERT_EQ(buffer[i], static_cast<uint8_t>(start + i * increment));
  }
}

// Instantiate all the types of MakeAudioBuffer() and
// MakeAudioBuffer() needed.
#define DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(type)              \
  template scoped_refptr<AudioBuffer> MakeAudioBuffer<type>( \
      SampleFormat format,                                   \
      ChannelLayout channel_layout,                          \
      size_t channel_count,                                  \
      int sample_rate,                                       \
      type start,                                            \
      type increment,                                        \
      size_t frames,                                         \
      base::TimeDelta start_time)
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(uint8_t);
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(int16_t);
DEFINE_MAKE_AUDIO_BUFFER_INSTANCE(int32_t);

static const char kFakeVideoBufferHeader[] = "FakeVideoBufferForTest";

scoped_refptr<DecoderBuffer> CreateFakeVideoBufferForTest(
    const VideoDecoderConfig& config,
    base::TimeDelta timestamp, base::TimeDelta duration) {
  base::Pickle pickle;
  pickle.WriteString(kFakeVideoBufferHeader);
  pickle.WriteInt(config.coded_size().width());
  pickle.WriteInt(config.coded_size().height());
  pickle.WriteInt64(timestamp.InMilliseconds());

  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom(static_cast<const uint8_t*>(pickle.data()),
                              static_cast<int>(pickle.size()));
  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);
  buffer->set_is_key_frame(true);

  return buffer;
}

scoped_refptr<DecoderBuffer> CreateMismatchedBufferForTest() {
  std::vector<uint8_t> data = {42, 22, 26, 13, 7, 16, 8, 2};
  scoped_refptr<media::DecoderBuffer> mismatched_encrypted_buffer =
      media::DecoderBuffer::CopyFrom(data.data(), data.size());
  mismatched_encrypted_buffer->set_timestamp(base::Seconds(42));
  mismatched_encrypted_buffer->set_duration(base::Seconds(64));
  mismatched_encrypted_buffer->set_decrypt_config(
      media::DecryptConfig::CreateCencConfig("fake_key_id", "fake_iv_16_bytes",
                                             {{1, 1}, {2, 2}, {3, 3}}));

  return mismatched_encrypted_buffer;
}

scoped_refptr<DecoderBuffer> CreateFakeEncryptedBuffer() {
  const int buffer_size = 16;  // Need a non-empty buffer;
  scoped_refptr<DecoderBuffer> buffer(
      base::MakeRefCounted<DecoderBuffer>(buffer_size));

  const uint8_t kFakeKeyId[] = {0x4b, 0x65, 0x79, 0x20, 0x49, 0x44};
  const uint8_t kFakeIv[DecryptConfig::kDecryptionKeySize] = {0};
  buffer->set_decrypt_config(DecryptConfig::CreateCencConfig(
      std::string(reinterpret_cast<const char*>(kFakeKeyId),
                  std::size(kFakeKeyId)),
      std::string(reinterpret_cast<const char*>(kFakeIv), std::size(kFakeIv)),
      std::vector<SubsampleEntry>()));
  return buffer;
}

scoped_refptr<DecoderBuffer> CreateClearBuffer() {
  const int buffer_size = 16;  // Need a non-empty buffer;
  auto buffer = base::MakeRefCounted<DecoderBuffer>(buffer_size);
  return buffer;
}

bool VerifyFakeVideoBufferForTest(const DecoderBuffer& buffer,
                                  const VideoDecoderConfig& config) {
  // Check if the input |buffer| matches the |config|.
  base::PickleIterator pickle(
      base::Pickle(reinterpret_cast<const char*>(buffer.data()),
                   static_cast<int>(buffer.data_size())));
  std::string header;
  int width = 0;
  int height = 0;
  bool success = pickle.ReadString(&header) && pickle.ReadInt(&width) &&
                 pickle.ReadInt(&height);
  return (success && header == kFakeVideoBufferHeader &&
          width == config.coded_size().width() &&
          height == config.coded_size().height());
}

std::unique_ptr<StrictMock<MockDemuxerStream>> CreateMockDemuxerStream(
    DemuxerStream::Type type,
    bool encrypted) {
  auto stream = std::make_unique<StrictMock<MockDemuxerStream>>(type);

  switch (type) {
    case DemuxerStream::AUDIO:
      stream->set_audio_decoder_config(encrypted
                                           ? TestAudioConfig::NormalEncrypted()
                                           : TestAudioConfig::Normal());
      break;
    case DemuxerStream::VIDEO:
      stream->set_video_decoder_config(encrypted
                                           ? TestVideoConfig::NormalEncrypted()
                                           : TestVideoConfig::Normal());
      break;
    default:
      NOTREACHED();
      break;
  }

  return stream;
}

}  // namespace media
