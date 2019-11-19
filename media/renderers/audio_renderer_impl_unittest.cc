// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/audio_renderer_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/fake_audio_renderer_sink.h"
#include "media/base/media_client.h"
#include "media/base/media_util.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::TimeDelta;
using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {

namespace {

// Since AudioBufferConverter is used due to different input/output sample
// rates, define some helper types to differentiate between the two.
struct InputFrames {
  explicit InputFrames(int value) : value(value) {}
  int value;
};

struct OutputFrames {
  explicit OutputFrames(int value) : value(value) {}
  int value;
};

}  // namespace

// Constants to specify the type of audio data used.
static AudioCodec kCodec = kCodecVorbis;
static SampleFormat kSampleFormat = kSampleFormatPlanarF32;
static ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static int kChannelCount = 2;
static int kChannels = ChannelLayoutToChannelCount(kChannelLayout);

// Use a different output sample rate so the AudioBufferConverter is invoked.
static int kInputSamplesPerSecond = 5000;
static int kOutputSamplesPerSecond = 10000;
static double kOutputMicrosPerFrame =
    static_cast<double>(base::Time::kMicrosecondsPerSecond) /
    kOutputSamplesPerSecond;

ACTION_P(EnterPendingDecoderInitStateAction, test) {
  test->EnterPendingDecoderInitState(std::move(arg2));
}

class AudioRendererImplTest : public ::testing::Test, public RendererClient {
 public:
  std::vector<std::unique_ptr<AudioDecoder>> CreateAudioDecoderForTest() {
    auto decoder = std::make_unique<MockAudioDecoder>();
    if (!enter_pending_decoder_init_) {
      EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _))
          .WillOnce(DoAll(SaveArg<3>(&output_cb_),
                          RunOnceCallback<2>(expected_init_result_)));
    } else {
      EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _))
          .WillOnce(EnterPendingDecoderInitStateAction(this));
    }
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillRepeatedly(Invoke(this, &AudioRendererImplTest::DecodeDecoder));
    EXPECT_CALL(*decoder, Reset_(_))
        .WillRepeatedly(Invoke(this, &AudioRendererImplTest::ResetDecoder));
    std::vector<std::unique_ptr<AudioDecoder>> decoders;
    decoders.push_back(std::move(decoder));
    return decoders;
  }

  // Give the decoder some non-garbage media properties.
  AudioRendererImplTest()
      : hardware_params_(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         kChannelLayout,
                         kOutputSamplesPerSecond,
                         512),
        main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        sink_(new FakeAudioRendererSink(hardware_params_)),
        demuxer_stream_(DemuxerStream::AUDIO),
        expected_init_result_(true),
        enter_pending_decoder_init_(false),
        ended_(false) {
    AudioDecoderConfig audio_config(kCodec, kSampleFormat, kChannelLayout,
                                    kInputSamplesPerSecond, EmptyExtraData(),
                                    EncryptionScheme::kUnencrypted);
    demuxer_stream_.set_audio_decoder_config(audio_config);

    ConfigureDemuxerStream(true);

    AudioParameters out_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               kChannelLayout,
                               kOutputSamplesPerSecond,
                               512);
    renderer_.reset(new AudioRendererImpl(
        main_thread_task_runner_, sink_.get(),
        base::BindRepeating(&AudioRendererImplTest::CreateAudioDecoderForTest,
                            base::Unretained(this)),
        &media_log_));
    renderer_->tick_clock_ = &tick_clock_;
    tick_clock_.Advance(base::TimeDelta::FromSeconds(1));
  }

  ~AudioRendererImplTest() override {
    SCOPED_TRACE("~AudioRendererImplTest()");
  }

  // Mock out demuxer reads.
  void ConfigureDemuxerStream(bool supports_config_changes) {
    EXPECT_CALL(demuxer_stream_, OnRead(_))
        .WillRepeatedly(RunOnceCallback<0>(
            DemuxerStream::kOk,
            scoped_refptr<DecoderBuffer>(new DecoderBuffer(0))));
    EXPECT_CALL(demuxer_stream_, SupportsConfigChanges())
        .WillRepeatedly(Return(supports_config_changes));
  }

  // Reconfigures a renderer without config change support using given params.
  void ConfigureBasicRenderer(const AudioParameters& params) {
    hardware_params_ = params;
    sink_ = new FakeAudioRendererSink(hardware_params_);
    renderer_.reset(new AudioRendererImpl(
        main_thread_task_runner_, sink_.get(),
        base::BindRepeating(&AudioRendererImplTest::CreateAudioDecoderForTest,
                            base::Unretained(this)),
        &media_log_));
    testing::Mock::VerifyAndClearExpectations(&demuxer_stream_);
    ConfigureDemuxerStream(false);
  }

  // Reconfigures a renderer with config change support using given params.
  void ConfigureConfigChangeRenderer(const AudioParameters& params,
                                     const AudioParameters& hardware_params) {
    hardware_params_ = hardware_params;
    sink_ = new FakeAudioRendererSink(hardware_params_);
    renderer_.reset(new AudioRendererImpl(
        main_thread_task_runner_, sink_.get(),
        base::BindRepeating(&AudioRendererImplTest::CreateAudioDecoderForTest,
                            base::Unretained(this)),
        &media_log_));
    testing::Mock::VerifyAndClearExpectations(&demuxer_stream_);
    ConfigureDemuxerStream(true);
  }

  void ConfigureMockRenderer(const AudioParameters& params) {
    mock_sink_ = new MockAudioRendererSink();
    renderer_.reset(new AudioRendererImpl(
        main_thread_task_runner_, mock_sink_.get(),
        base::BindRepeating(&AudioRendererImplTest::CreateAudioDecoderForTest,
                            base::Unretained(this)),
        &media_log_));
    testing::Mock::VerifyAndClearExpectations(&demuxer_stream_);
    ConfigureDemuxerStream(true);
  }

  // RendererClient implementation.
  MOCK_METHOD1(OnError, void(PipelineStatus));
  void OnEnded() override {
    CHECK(!ended_);
    ended_ = true;
  }
  void OnStatisticsUpdate(const PipelineStatistics& stats) override {
    last_statistics_.audio_memory_usage += stats.audio_memory_usage;
  }
  MOCK_METHOD2(OnBufferingStateChange,
               void(BufferingState, BufferingStateChangeReason));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size&));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool));
  MOCK_METHOD1(OnDurationChange, void(base::TimeDelta));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));

  void InitializeRenderer(DemuxerStream* demuxer_stream,
                          const PipelineStatusCB& pipeline_status_cb) {
    EXPECT_CALL(*this, OnWaiting(_)).Times(0);
    EXPECT_CALL(*this, OnVideoNaturalSizeChange(_)).Times(0);
    EXPECT_CALL(*this, OnVideoOpacityChange(_)).Times(0);
    EXPECT_CALL(*this, OnVideoConfigChange(_)).Times(0);
    renderer_->Initialize(demuxer_stream, nullptr, this, pipeline_status_cb);
  }

  void Initialize() {
    InitializeWithStatus(PIPELINE_OK);

    next_timestamp_.reset(new AudioTimestampHelper(kInputSamplesPerSecond));
  }

  void InitializeBitstreamFormat() {
    EXPECT_CALL(media_client_, IsSupportedBitstreamAudioCodec(_))
        .WillRepeatedly(Return(true));
    SetMediaClient(&media_client_);

    hardware_params_.Reset(AudioParameters::AUDIO_BITSTREAM_EAC3,
                           kChannelLayout, kOutputSamplesPerSecond, 512);
    sink_ = new FakeAudioRendererSink(hardware_params_);
    AudioDecoderConfig audio_config(
        kCodecAC3, kSampleFormatEac3, kChannelLayout, kInputSamplesPerSecond,
        EmptyExtraData(), EncryptionScheme::kUnencrypted);
    demuxer_stream_.set_audio_decoder_config(audio_config);

    ConfigureDemuxerStream(true);

    renderer_.reset(new AudioRendererImpl(
        main_thread_task_runner_, sink_.get(),
        base::BindRepeating(&AudioRendererImplTest::CreateAudioDecoderForTest,
                            base::Unretained(this)),
        &media_log_));

    Initialize();
  }

  void InitializeWithStatus(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("InitializeWithStatus(%d)", expected));

    WaitableMessageLoopEvent event;
    InitializeRenderer(&demuxer_stream_, event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(expected);

    // We should have no reads.
    EXPECT_TRUE(!decode_cb_);
  }

  void InitializeAndDestroy() {
    WaitableMessageLoopEvent event;
    InitializeRenderer(&demuxer_stream_, event.GetPipelineStatusCB());

    // Destroy the |renderer_| before we let the MessageLoop run, this simulates
    // an interleaving in which we end up destroying the |renderer_| while the
    // OnDecoderSelected callback is in flight.
    renderer_.reset();
    event.RunAndWaitForStatus(PIPELINE_ERROR_ABORT);
  }

  void InitializeAndDestroyDuringDecoderInit() {
    enter_pending_decoder_init_ = true;

    WaitableMessageLoopEvent event;
    InitializeRenderer(&demuxer_stream_, event.GetPipelineStatusCB());
    base::RunLoop().RunUntilIdle();
    DCHECK(init_decoder_cb_);

    renderer_.reset();
    event.RunAndWaitForStatus(PIPELINE_ERROR_ABORT);
  }

  void EnterPendingDecoderInitState(AudioDecoder::InitCB cb) {
    init_decoder_cb_ = std::move(cb);
  }

  void FlushDuringPendingRead() {
    SCOPED_TRACE("FlushDuringPendingRead()");
    WaitableMessageLoopEvent flush_event;
    renderer_->Flush(flush_event.GetClosure());
    SatisfyPendingRead(InputFrames(256));
    flush_event.RunAndWait();

    EXPECT_FALSE(IsReadPending());
  }

  void Preroll() {
    Preroll(base::TimeDelta(), base::TimeDelta(), PIPELINE_OK);
  }

  void Preroll(base::TimeDelta start_timestamp,
               base::TimeDelta first_timestamp,
               PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("Preroll(%" PRId64 ", %d)",
                                    first_timestamp.InMilliseconds(),
                                    expected));
    next_timestamp_->SetBaseTimestamp(first_timestamp);

    // Fill entire buffer to complete prerolling.
    renderer_->SetMediaTime(start_timestamp);
    renderer_->StartPlaying();
    WaitForPendingRead();
    EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                              BUFFERING_CHANGE_REASON_UNKNOWN));
    DeliverRemainingAudio();
  }

  void StartTicking() {
    renderer_->StartTicking();
    renderer_->SetPlaybackRate(1.0);
  }

  void StopTicking() { renderer_->StopTicking(); }

  bool IsReadPending() const { return !!decode_cb_; }

  void WaitForPendingRead() {
    SCOPED_TRACE("WaitForPendingRead()");
    if (decode_cb_)
      return;

    DCHECK(!wait_for_pending_decode_cb_);

    WaitableMessageLoopEvent event;
    wait_for_pending_decode_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(decode_cb_);
    DCHECK(!wait_for_pending_decode_cb_);
  }

  // Delivers decoded frames to |renderer_|.
  void SatisfyPendingRead(InputFrames frames) {
    CHECK_GT(frames.value, 0);
    CHECK(decode_cb_);

    scoped_refptr<AudioBuffer> buffer;
    if (hardware_params_.IsBitstreamFormat()) {
      buffer = MakeBitstreamAudioBuffer(kSampleFormatEac3, kChannelLayout,
                                        kChannelCount, kInputSamplesPerSecond,
                                        1, 0, frames.value, frames.value / 2,
                                        next_timestamp_->GetTimestamp());
    } else {
      buffer = MakeAudioBuffer<float>(
          kSampleFormat, kChannelLayout, kChannelCount, kInputSamplesPerSecond,
          1.0f, 0.0f, frames.value, next_timestamp_->GetTimestamp());
    }
    next_timestamp_->AddFrames(frames.value);

    DeliverBuffer(DecodeStatus::OK, std::move(buffer));
  }

  void DeliverEndOfStream() {
    DCHECK(decode_cb_);

    // Return EOS buffer to trigger EOS frame.
    EXPECT_CALL(demuxer_stream_, OnRead(_))
        .WillOnce(RunOnceCallback<0>(DemuxerStream::kOk,
                                     DecoderBuffer::CreateEOSBuffer()));

    // Satify pending |decode_cb_| to trigger a new DemuxerStream::Read().
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(decode_cb_), DecodeStatus::OK));

    WaitForPendingRead();

    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(decode_cb_), DecodeStatus::OK));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(last_statistics_.audio_memory_usage,
              renderer_->algorithm_->GetMemoryUsage());
  }

  // Delivers frames until |renderer_|'s internal buffer is full and no longer
  // has pending reads.
  void DeliverRemainingAudio() {
    while (frames_remaining_in_buffer().value > 0) {
      SatisfyPendingRead(InputFrames(256));
    }
  }

  // Attempts to consume |requested_frames| frames from |renderer_|'s internal
  // buffer. Returns true if and only if all of |requested_frames| were able
  // to be consumed.
  bool ConsumeBufferedData(OutputFrames requested_frames,
                           base::TimeDelta delay) {
    std::unique_ptr<AudioBus> bus =
        AudioBus::Create(kChannels, requested_frames.value);
    int frames_read = 0;
    EXPECT_TRUE(sink_->Render(bus.get(), delay, &frames_read));
    return frames_read == requested_frames.value;
  }

  bool ConsumeBufferedData(OutputFrames requested_frames) {
    return ConsumeBufferedData(requested_frames, base::TimeDelta());
  }

  bool ConsumeBitstreamBufferedData(OutputFrames requested_frames,
                                    base::TimeDelta delay = base::TimeDelta()) {
    std::unique_ptr<AudioBus> bus =
        AudioBus::Create(kChannels, requested_frames.value);
    int total_frames_read = 0;
    while (total_frames_read != requested_frames.value) {
      int frames_read = 0;
      EXPECT_TRUE(sink_->Render(bus.get(), delay, &frames_read));

      if (frames_read <= 0)
        break;
      total_frames_read += frames_read;
    }

    return total_frames_read == requested_frames.value;
  }

  base::TimeTicks ConvertMediaTime(base::TimeDelta timestamp,
                                   bool* is_time_moving) {
    std::vector<base::TimeTicks> wall_clock_times;
    *is_time_moving = renderer_->GetWallClockTimes(
        std::vector<base::TimeDelta>(1, timestamp), &wall_clock_times);
    return wall_clock_times[0];
  }

  base::TimeTicks CurrentMediaWallClockTime(bool* is_time_moving) {
    std::vector<base::TimeTicks> wall_clock_times;
    *is_time_moving = renderer_->GetWallClockTimes(
        std::vector<base::TimeDelta>(), &wall_clock_times);
    return wall_clock_times[0];
  }

  OutputFrames frames_buffered() {
    return OutputFrames(renderer_->algorithm_->frames_buffered());
  }

  OutputFrames buffer_capacity() {
    return OutputFrames(renderer_->algorithm_->QueueCapacity());
  }

  OutputFrames frames_remaining_in_buffer() {
    // This can happen if too much data was delivered, in which case the buffer
    // will accept the data but not increase capacity.
    if (frames_buffered().value > buffer_capacity().value) {
      return OutputFrames(0);
    }
    return OutputFrames(buffer_capacity().value - frames_buffered().value);
  }

  void force_config_change(const AudioDecoderConfig& config) {
    renderer_->OnConfigChange(config);
  }

  InputFrames converter_input_frames_left() const {
    return InputFrames(
        renderer_->buffer_converter_->input_frames_left_for_testing());
  }

  base::TimeDelta CurrentMediaTime() {
    return renderer_->CurrentMediaTime();
  }

  std::vector<bool> channel_mask() const {
    CHECK(renderer_->algorithm_);
    return renderer_->algorithm_->channel_mask_for_testing();
  }

  bool ended() const { return ended_; }

  void DecodeDecoder(scoped_refptr<DecoderBuffer> buffer,
                     const AudioDecoder::DecodeCB& decode_cb) {
    // TODO(scherkus): Make this a DCHECK after threading semantics are fixed.
    if (!main_thread_task_runner_->BelongsToCurrentThread()) {
      main_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&AudioRendererImplTest::DecodeDecoder,
                                    base::Unretained(this), buffer, decode_cb));
      return;
    }

    CHECK(!decode_cb_) << "Overlapping decodes are not permitted";
    decode_cb_ = decode_cb;

    // Wake up WaitForPendingRead() if needed.
    if (wait_for_pending_decode_cb_)
      std::move(wait_for_pending_decode_cb_).Run();
  }

  void ResetDecoder(base::OnceClosure& reset_cb) {
    if (decode_cb_) {
      // |reset_cb| will be called in DeliverBuffer(), after the decoder is
      // flushed.
      reset_cb_ = std::move(reset_cb);
      return;
    }

    main_thread_task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
  }

  void DeliverBuffer(DecodeStatus status, scoped_refptr<AudioBuffer> buffer) {
    CHECK(decode_cb_);

    if (buffer.get() && !buffer->end_of_stream())
      output_cb_.Run(std::move(buffer));
    std::move(decode_cb_).Run(status);

    if (reset_cb_)
      std::move(reset_cb_).Run();

    base::RunLoop().RunUntilIdle();
  }

  // Fixture members.
  AudioParameters hardware_params_;
  base::test::TaskEnvironment task_environment_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  NullMediaLog media_log_;
  std::unique_ptr<AudioRendererImpl> renderer_;
  scoped_refptr<FakeAudioRendererSink> sink_;
  scoped_refptr<MockAudioRendererSink> mock_sink_;
  base::SimpleTestTickClock tick_clock_;
  PipelineStatistics last_statistics_;

  MockDemuxerStream demuxer_stream_;
  MockMediaClient media_client_;

  // Used for satisfying reads.
  AudioDecoder::OutputCB output_cb_;
  AudioDecoder::DecodeCB decode_cb_;
  base::OnceClosure reset_cb_;
  std::unique_ptr<AudioTimestampHelper> next_timestamp_;

  // Run during DecodeDecoder() to unblock WaitForPendingRead().
  base::Closure wait_for_pending_decode_cb_;

  AudioDecoder::InitCB init_decoder_cb_;
  bool expected_init_result_;
  bool enter_pending_decoder_init_;
  bool ended_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImplTest);
};

TEST_F(AudioRendererImplTest, Initialize_Successful) {
  Initialize();
}

TEST_F(AudioRendererImplTest, Initialize_DecoderInitFailure) {
  expected_init_result_ = false;
  InitializeWithStatus(DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(AudioRendererImplTest, ReinitializeForDifferentStream) {
  // Initialize and start playback
  Initialize();
  Preroll();
  StartTicking();
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  // Stop playback and flush
  StopTicking();
  EXPECT_TRUE(IsReadPending());
  // Flush and expect to be notified that we have nothing.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  FlushDuringPendingRead();

  // Prepare a new demuxer stream.
  MockDemuxerStream new_stream(DemuxerStream::AUDIO);
  EXPECT_CALL(new_stream, SupportsConfigChanges()).WillOnce(Return(false));
  AudioDecoderConfig audio_config(kCodec, kSampleFormat, kChannelLayout,
                                  kInputSamplesPerSecond, EmptyExtraData(),
                                  EncryptionScheme::kUnencrypted);
  new_stream.set_audio_decoder_config(audio_config);

  // The renderer is now in the flushed state and can be reinitialized.
  WaitableMessageLoopEvent event;
  InitializeRenderer(&new_stream, event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);
}

TEST_F(AudioRendererImplTest, SignalConfigChange) {
  // Initialize and start playback.
  Initialize();
  Preroll();
  StartTicking();
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  // Force config change to simulate detected change from decoder stream. Expect
  // that RendererClient to be signaled with the new config.
  const AudioDecoderConfig kValidAudioConfig(
      kCodecVorbis, kSampleFormatPlanarF32, CHANNEL_LAYOUT_STEREO, 44100,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_TRUE(kValidAudioConfig.IsValidConfig());
  EXPECT_CALL(*this, OnAudioConfigChange(DecoderConfigEq(kValidAudioConfig)));
  force_config_change(kValidAudioConfig);

  // Verify rendering can continue after config change.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  // Force a config change with an invalid dummy config. This is occasionally
  // done to reset internal state and should not bubble to the RendererClient.
  EXPECT_CALL(*this, OnAudioConfigChange(_)).Times(0);
  const AudioDecoderConfig kInvalidConfig;
  EXPECT_FALSE(kInvalidConfig.IsValidConfig());
  force_config_change(kInvalidConfig);
}

TEST_F(AudioRendererImplTest, Preroll) {
  Initialize();
  Preroll();
}

TEST_F(AudioRendererImplTest, StartTicking) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();
}

TEST_F(AudioRendererImplTest, EndOfStream) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Forcefully trigger underflow.
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));

  // Fulfill the read with an end-of-stream buffer. Doing so should change our
  // buffering state so playback resumes.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                            BUFFERING_CHANGE_REASON_UNKNOWN));
  DeliverEndOfStream();

  // Consume all remaining data. We shouldn't have signal ended yet.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ended());

  // Ended should trigger on next render call.
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ended());
}

TEST_F(AudioRendererImplTest, DecoderUnderflow) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers a buffering state change
  // update. Expect a decoder underflow flag because demuxer is not blocked in a
  // pending read.
  EXPECT_CALL(
      *this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, DECODER_UNDERFLOW));
  EXPECT_CALL(demuxer_stream_, IsReadPending()).WillOnce(Return(false));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));

  // Verify we're still not getting audio data.
  EXPECT_EQ(0, frames_buffered().value);
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));

  // Deliver enough data to have enough for buffering.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                            BUFFERING_CHANGE_REASON_UNKNOWN));
  DeliverRemainingAudio();

  // Verify we're getting audio data.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(1)));
}

TEST_F(AudioRendererImplTest, DemuxerUnderflow) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers a buffering state change
  // update. Expect a decoder underflow flag because demuxer is not blocked in a
  // pending read.
  EXPECT_CALL(
      *this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, DEMUXER_UNDERFLOW));
  EXPECT_CALL(demuxer_stream_, IsReadPending()).WillOnce(Return(true));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));

  // Verify we're still not getting audio data.
  EXPECT_EQ(0, frames_buffered().value);
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));

  // Deliver enough data to have enough for buffering.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                            BUFFERING_CHANGE_REASON_UNKNOWN));
  DeliverRemainingAudio();

  // Verify we're getting audio data.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(1)));
}

TEST_F(AudioRendererImplTest, Underflow_CapacityResetsAfterFlush) {
  Initialize();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  OutputFrames initial_capacity = buffer_capacity();
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));

  // Verify that the buffer capacity increased as a result of underflowing.
  EXPECT_GT(buffer_capacity().value, initial_capacity.value);

  // Verify that the buffer capacity is restored to the |initial_capacity|.
  StopTicking();
  FlushDuringPendingRead();
  EXPECT_EQ(buffer_capacity().value, initial_capacity.value);
}

TEST_F(AudioRendererImplTest, Underflow_CapacityIncreasesBeforeHaveNothing) {
  Initialize();
  Preroll();
  StartTicking();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  OutputFrames initial_capacity = buffer_capacity();

  // Drain internal buffer, we should have a pending read.
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(frames_buffered().value + 1)));

  // Verify that the buffer capacity increased despite not sending have nothing.
  EXPECT_GT(buffer_capacity().value, initial_capacity.value);
}

TEST_F(AudioRendererImplTest, Underflow_OneCapacityIncreasePerUnderflow) {
  Initialize();
  Preroll();
  StartTicking();

  OutputFrames prev_capacity = buffer_capacity();

  // Consume more than is available (partial read) to trigger underflow.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(frames_buffered().value + 1)));

  // Verify first underflow triggers an increase to buffer capacity and
  // signals HAVE_NOTHING.
  EXPECT_GT(buffer_capacity().value, prev_capacity.value);
  prev_capacity = buffer_capacity();
  // Give HAVE_NOTHING a chance to post.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  // Try reading again, this time with the queue totally empty. We should expect
  // NO additional HAVE_NOTHING and NO increase to capacity because we still
  // haven't refilled the queue since the previous underflow.
  EXPECT_EQ(0, frames_buffered().value);
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _))
      .Times(0);
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  EXPECT_EQ(buffer_capacity().value, prev_capacity.value);
  // Give HAVE_NOTHING a chance to NOT post.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  // Fill the buffer back up.
  WaitForPendingRead();
  DeliverRemainingAudio();
  EXPECT_GT(frames_buffered().value, 0);

  // Consume all available data without underflowing. Expect no buffer state
  // change and no change to capacity.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _))
      .Times(0);
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(frames_buffered().value)));
  EXPECT_EQ(buffer_capacity().value, prev_capacity.value);
  // Give HAVE_NOTHING a chance to NOT post.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  // Now empty, trigger underflow attempting to read one frame. This should
  // signal buffering state change and increase capacity.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  EXPECT_GT(buffer_capacity().value, prev_capacity.value);
  // Give HAVE_NOTHING a chance to NOT post.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);
}

// Verify that the proper reduced search space is configured for playback rate
// changes when upmixing is applied to the input.
TEST_F(AudioRendererImplTest, ChannelMask) {
  AudioParameters hw_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                            CHANNEL_LAYOUT_7_1, kOutputSamplesPerSecond,
                            1024);
  ConfigureConfigChangeRenderer(
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      CHANNEL_LAYOUT_STEREO, kOutputSamplesPerSecond, 1024),
      hw_params);
  Initialize();
  std::vector<bool> mask = channel_mask();
  EXPECT_FALSE(mask.empty());
  ASSERT_EQ(mask.size(), static_cast<size_t>(hw_params.channels()));
  for (int ch = 0; ch < hw_params.channels(); ++ch) {
    if (ch > 1)
      ASSERT_FALSE(mask[ch]);
    else
      ASSERT_TRUE(mask[ch]);
  }

  renderer_->SetMediaTime(base::TimeDelta());
  renderer_->StartPlaying();
  WaitForPendingRead();

  // Force a channel configuration change.
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<float>(
      kSampleFormat, hw_params.channel_layout(), hw_params.channels(),
      kInputSamplesPerSecond, 1.0f, 0.0f, 256, base::TimeDelta());
  DeliverBuffer(DecodeStatus::OK, std::move(buffer));

  // All channels should now be enabled.
  mask = channel_mask();
  EXPECT_FALSE(mask.empty());
  ASSERT_EQ(mask.size(), static_cast<size_t>(hw_params.channels()));
  for (int ch = 0; ch < hw_params.channels(); ++ch)
    ASSERT_TRUE(mask[ch]);
}

// Verify that the proper channel mask is configured when downmixing is applied
// to the input with discrete layout. The default hardware layout is stereo.
TEST_F(AudioRendererImplTest, ChannelMask_DownmixDiscreteLayout) {
  int audio_channels = 9;

  AudioDecoderConfig audio_config(
      kCodecOpus, kSampleFormat, CHANNEL_LAYOUT_DISCRETE,
      kInputSamplesPerSecond, EmptyExtraData(), EncryptionScheme::kUnencrypted);
  audio_config.SetChannelsForDiscrete(audio_channels);
  demuxer_stream_.set_audio_decoder_config(audio_config);
  ConfigureDemuxerStream(true);

  // Fake an attached webaudio client.
  sink_->SetIsOptimizedForHardwareParameters(false);

  Initialize();
  std::vector<bool> mask = channel_mask();
  EXPECT_FALSE(mask.empty());
  ASSERT_EQ(mask.size(), static_cast<size_t>(audio_channels));
  for (int ch = 0; ch < audio_channels; ++ch)
    ASSERT_TRUE(mask[ch]);
}

TEST_F(AudioRendererImplTest, Underflow_Flush) {
  Initialize();
  Preroll();
  StartTicking();

  // Force underflow.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered()));
  WaitForPendingRead();
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  WaitForPendingRead();
  StopTicking();

  // We shouldn't expect another buffering state change when flushing.
  FlushDuringPendingRead();
}

TEST_F(AudioRendererImplTest, PendingRead_Flush) {
  Initialize();

  Preroll();
  StartTicking();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  StopTicking();

  EXPECT_TRUE(IsReadPending());

  // Flush and expect to be notified that we have nothing.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  FlushDuringPendingRead();

  // Preroll again to a different timestamp and verify it completed normally.
  const base::TimeDelta seek_timestamp =
      base::TimeDelta::FromMilliseconds(1000);
  Preroll(seek_timestamp, seek_timestamp, PIPELINE_OK);
}

TEST_F(AudioRendererImplTest, PendingRead_Destroy) {
  Initialize();

  Preroll();
  StartTicking();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  StopTicking();

  EXPECT_TRUE(IsReadPending());

  renderer_.reset();
}

TEST_F(AudioRendererImplTest, PendingFlush_Destroy) {
  Initialize();

  Preroll();
  StartTicking();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(256)));
  WaitForPendingRead();

  StopTicking();

  EXPECT_TRUE(IsReadPending());

  // Start flushing.
  WaitableMessageLoopEvent flush_event;
  renderer_->Flush(flush_event.GetClosure());

  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));
  SatisfyPendingRead(InputFrames(256));

  renderer_.reset();
}

TEST_F(AudioRendererImplTest, InitializeThenDestroy) {
  InitializeAndDestroy();
}

TEST_F(AudioRendererImplTest, InitializeThenDestroyDuringDecoderInit) {
  InitializeAndDestroyDuringDecoderInit();
}

TEST_F(AudioRendererImplTest, CurrentMediaTimeBehavior) {
  Initialize();
  Preroll();
  StartTicking();

  AudioTimestampHelper timestamp_helper(kOutputSamplesPerSecond);
  timestamp_helper.SetBaseTimestamp(base::TimeDelta());

  // Time should be the starting timestamp as nothing has been consumed yet.
  EXPECT_EQ(timestamp_helper.GetTimestamp(), CurrentMediaTime());

  const OutputFrames frames_to_consume(frames_buffered().value / 3);
  const base::TimeDelta kConsumptionDuration =
      timestamp_helper.GetFrameDuration(frames_to_consume.value);

  // Render() has not be called yet, thus no data has been consumed, so
  // advancing tick clock must not change the media time.
  tick_clock_.Advance(kConsumptionDuration);
  EXPECT_EQ(timestamp_helper.GetTimestamp(), CurrentMediaTime());

  // Consume some audio data.
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume));
  WaitForPendingRead();

  // Time shouldn't change just yet because we've only sent the initial audio
  // data to the hardware.
  EXPECT_EQ(timestamp_helper.GetTimestamp(), CurrentMediaTime());

  // Advancing the tick clock now should result in an estimated media time.
  tick_clock_.Advance(kConsumptionDuration);
  EXPECT_EQ(timestamp_helper.GetTimestamp() + kConsumptionDuration,
            CurrentMediaTime());

  // Consume some more audio data.
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume));

  // Time should change now that Render() has been called a second time.
  timestamp_helper.AddFrames(frames_to_consume.value);
  EXPECT_EQ(timestamp_helper.GetTimestamp(), CurrentMediaTime());

  // Advance current time well past all played audio to simulate an irregular or
  // delayed OS callback. The value should be clamped to whats been rendered.
  timestamp_helper.AddFrames(frames_to_consume.value);
  tick_clock_.Advance(kConsumptionDuration * 2);
  EXPECT_EQ(timestamp_helper.GetTimestamp(), CurrentMediaTime());

  // Consume some more audio data.
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume));

  // Stop ticking, the media time should be clamped to what's been rendered.
  StopTicking();
  EXPECT_EQ(timestamp_helper.GetTimestamp(), CurrentMediaTime());
  tick_clock_.Advance(kConsumptionDuration * 2);
  timestamp_helper.AddFrames(frames_to_consume.value);
  EXPECT_EQ(timestamp_helper.GetTimestamp(), CurrentMediaTime());
}

TEST_F(AudioRendererImplTest, RenderingDelayedForEarlyStartTime) {
  Initialize();

  // Choose a first timestamp a few buffers into the future, which ends halfway
  // through the desired output buffer; this allows for maximum test coverage.
  const double kBuffers = 4.5;
  const base::TimeDelta first_timestamp =
      base::TimeDelta::FromSecondsD(hardware_params_.frames_per_buffer() *
                                    kBuffers / hardware_params_.sample_rate());

  Preroll(base::TimeDelta(), first_timestamp, PIPELINE_OK);
  StartTicking();

  // Verify the first few buffers are silent.
  std::unique_ptr<AudioBus> bus = AudioBus::Create(hardware_params_);
  int frames_read = 0;
  for (int i = 0; i < std::floor(kBuffers); ++i) {
    EXPECT_TRUE(sink_->Render(bus.get(), base::TimeDelta(), &frames_read));
    EXPECT_EQ(frames_read, bus->frames());
    for (int j = 0; j < bus->frames(); ++j)
      ASSERT_FLOAT_EQ(0.0f, bus->channel(0)[j]);
    WaitForPendingRead();
    DeliverRemainingAudio();
  }

  // Verify the last buffer is half silence and half real data.
  EXPECT_TRUE(sink_->Render(bus.get(), base::TimeDelta(), &frames_read));
  EXPECT_EQ(frames_read, bus->frames());
  const int zero_frames =
      bus->frames() * (kBuffers - static_cast<int>(kBuffers));

  for (int i = 0; i < zero_frames; ++i)
    ASSERT_FLOAT_EQ(0.0f, bus->channel(0)[i]);
  for (int i = zero_frames; i < bus->frames(); ++i)
    ASSERT_NE(0.0f, bus->channel(0)[i]);
}

TEST_F(AudioRendererImplTest, RenderingDelayedForSuspend) {
  Initialize();
  Preroll(base::TimeDelta(), base::TimeDelta(), PIPELINE_OK);
  StartTicking();

  // Verify the first buffer is real data.
  int frames_read = 0;
  std::unique_ptr<AudioBus> bus = AudioBus::Create(hardware_params_);
  EXPECT_TRUE(sink_->Render(bus.get(), base::TimeDelta(), &frames_read));
  EXPECT_NE(0, frames_read);
  for (int i = 0; i < bus->frames(); ++i)
    ASSERT_NE(0.0f, bus->channel(0)[i]);

  // Verify after suspend we get silence.
  renderer_->OnSuspend();
  EXPECT_TRUE(sink_->Render(bus.get(), base::TimeDelta(), &frames_read));
  EXPECT_EQ(0, frames_read);

  // Verify after resume we get audio.
  bus->Zero();
  renderer_->OnResume();
  EXPECT_TRUE(sink_->Render(bus.get(), base::TimeDelta(), &frames_read));
  EXPECT_NE(0, frames_read);
  for (int i = 0; i < bus->frames(); ++i)
    ASSERT_NE(0.0f, bus->channel(0)[i]);
}

TEST_F(AudioRendererImplTest, RenderingDelayDoesNotOverflow) {
  Initialize();

  // Choose a first timestamp as far into the future as possible. Without care
  // this can cause an overflow in rendering arithmetic.
  Preroll(base::TimeDelta(), base::TimeDelta::Max(), PIPELINE_OK);
  StartTicking();
  EXPECT_TRUE(ConsumeBufferedData(OutputFrames(1)));
}

TEST_F(AudioRendererImplTest, ImmediateEndOfStream) {
  Initialize();
  {
    SCOPED_TRACE("Preroll()");
    renderer_->StartPlaying();
    WaitForPendingRead();
    EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                              BUFFERING_CHANGE_REASON_UNKNOWN));
    DeliverEndOfStream();
  }
  StartTicking();

  // Read a single frame. We shouldn't be able to satisfy it.
  EXPECT_FALSE(ended());
  EXPECT_FALSE(ConsumeBufferedData(OutputFrames(1)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ended());
}

TEST_F(AudioRendererImplTest, OnRenderErrorCausesDecodeError) {
  Initialize();
  Preroll();
  StartTicking();

  EXPECT_CALL(*this, OnError(AUDIO_RENDERER_ERROR));
  sink_->OnRenderError();
  base::RunLoop().RunUntilIdle();
}

// Test for AudioRendererImpl calling Pause()/Play() on the sink when the
// playback rate is set to zero and non-zero.
TEST_F(AudioRendererImplTest, SetPlaybackRate) {
  Initialize();
  Preroll();

  // Rendering hasn't started. Sink should always be paused.
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(0.0);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());

  // Rendering has started with non-zero rate. Rate changes will affect sink
  // state.
  renderer_->StartTicking();
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());
  renderer_->SetPlaybackRate(0.0);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0);
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());

  // Rendering has stopped. Sink should be paused.
  renderer_->StopTicking();
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());

  // Start rendering with zero playback rate. Sink should be paused until
  // non-zero rate is set.
  renderer_->SetPlaybackRate(0.0);
  renderer_->StartTicking();
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0);
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());
}

TEST_F(AudioRendererImplTest, TimeSourceBehavior) {
  Initialize();
  Preroll();

  AudioTimestampHelper timestamp_helper(kOutputSamplesPerSecond);
  timestamp_helper.SetBaseTimestamp(base::TimeDelta());

  // Prior to start, time should be shown as not moving.
  bool is_time_moving = false;
  EXPECT_EQ(base::TimeTicks(),
            ConvertMediaTime(base::TimeDelta(), &is_time_moving));
  EXPECT_FALSE(is_time_moving);

  EXPECT_EQ(base::TimeTicks(), CurrentMediaWallClockTime(&is_time_moving));
  EXPECT_FALSE(is_time_moving);

  // Start ticking, but use a zero playback rate, time should still be stopped
  // until a positive playback rate is set and the first Render() is called.
  renderer_->SetPlaybackRate(0.0);
  StartTicking();
  EXPECT_EQ(base::TimeTicks(), CurrentMediaWallClockTime(&is_time_moving));
  EXPECT_FALSE(is_time_moving);
  renderer_->SetPlaybackRate(1.0);
  EXPECT_EQ(base::TimeTicks(), CurrentMediaWallClockTime(&is_time_moving));
  EXPECT_FALSE(is_time_moving);
  renderer_->SetPlaybackRate(1.0);

  // Issue the first render call to start time moving.
  OutputFrames frames_to_consume(frames_buffered().value / 2);
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume));
  WaitForPendingRead();

  // Time shouldn't change just yet because we've only sent the initial audio
  // data to the hardware.
  EXPECT_EQ(tick_clock_.NowTicks(),
            ConvertMediaTime(base::TimeDelta(), &is_time_moving));
  EXPECT_TRUE(is_time_moving);

  // A system suspend should freeze the time state and resume restart it.
  renderer_->OnSuspend();
  EXPECT_EQ(tick_clock_.NowTicks(),
            ConvertMediaTime(base::TimeDelta(), &is_time_moving));
  EXPECT_FALSE(is_time_moving);
  renderer_->OnResume();
  EXPECT_EQ(tick_clock_.NowTicks(),
            ConvertMediaTime(base::TimeDelta(), &is_time_moving));
  EXPECT_TRUE(is_time_moving);

  // Consume some more audio data.
  frames_to_consume = frames_buffered();
  tick_clock_.Advance(
      base::TimeDelta::FromSecondsD(1.0 / kOutputSamplesPerSecond));
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume));

  // Time should change now that the audio hardware has called back.
  const base::TimeTicks wall_clock_time_zero =
      tick_clock_.NowTicks() -
      timestamp_helper.GetFrameDuration(frames_to_consume.value);
  EXPECT_EQ(wall_clock_time_zero,
            ConvertMediaTime(base::TimeDelta(), &is_time_moving));
  EXPECT_TRUE(is_time_moving);

  // Store current media time before advancing the tick clock since the call is
  // compensated based on TimeTicks::Now().
  const base::TimeDelta current_media_time = renderer_->CurrentMediaTime();

  // The current wall clock time should change as our tick clock advances, up
  // until we've reached the end of played out frames.
  const int kSteps = 4;
  const base::TimeDelta kAdvanceDelta =
      timestamp_helper.GetFrameDuration(frames_to_consume.value) / kSteps;

  for (int i = 0; i < kSteps; ++i) {
    tick_clock_.Advance(kAdvanceDelta);
    EXPECT_EQ(tick_clock_.NowTicks(),
              CurrentMediaWallClockTime(&is_time_moving));
    EXPECT_TRUE(is_time_moving);
  }

  // Converting the current media time should be relative to wall clock zero.
  EXPECT_EQ(wall_clock_time_zero + kSteps * kAdvanceDelta,
            ConvertMediaTime(current_media_time, &is_time_moving));
  EXPECT_TRUE(is_time_moving);

  // Advancing once more will exceed the amount of played out frames finally.
  const base::TimeDelta kOneSample =
      base::TimeDelta::FromSecondsD(1.0 / kOutputSamplesPerSecond);
  base::TimeTicks current_time = tick_clock_.NowTicks();
  tick_clock_.Advance(kOneSample);
  EXPECT_EQ(current_time, CurrentMediaWallClockTime(&is_time_moving));
  EXPECT_TRUE(is_time_moving);

  StopTicking();
  DeliverRemainingAudio();

  // Elapse a lot of time between StopTicking() and the next Render() call.
  const base::TimeDelta kOneSecond = base::TimeDelta::FromSeconds(1);
  tick_clock_.Advance(kOneSecond);
  StartTicking();

  // Time should be stopped until the next render call.
  EXPECT_EQ(current_time, CurrentMediaWallClockTime(&is_time_moving));
  EXPECT_FALSE(is_time_moving);

  // Consume some buffered data with a small delay.
  uint32_t delay_frames = 500;
  base::TimeDelta delay_time = base::TimeDelta::FromMicroseconds(
      std::round(delay_frames * kOutputMicrosPerFrame));

  frames_to_consume.value = frames_buffered().value / 16;
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume, delay_time));

  // Verify time is adjusted for the current delay.
  current_time = tick_clock_.NowTicks() + delay_time;
  EXPECT_EQ(current_time, CurrentMediaWallClockTime(&is_time_moving));
  EXPECT_TRUE(is_time_moving);
  EXPECT_EQ(current_time,
            ConvertMediaTime(renderer_->CurrentMediaTime(), &is_time_moving));
  EXPECT_TRUE(is_time_moving);

  tick_clock_.Advance(kOneSample);
  renderer_->SetPlaybackRate(2);
  EXPECT_EQ(current_time, CurrentMediaWallClockTime(&is_time_moving));
  EXPECT_TRUE(is_time_moving);
  EXPECT_EQ(current_time + kOneSample * 2,
            ConvertMediaTime(renderer_->CurrentMediaTime(), &is_time_moving));
  EXPECT_TRUE(is_time_moving);

  // Advance far enough that we shouldn't be clamped to current time (tested
  // already above).
  tick_clock_.Advance(kOneSecond);
  EXPECT_EQ(
      current_time + timestamp_helper.GetFrameDuration(frames_to_consume.value),
      CurrentMediaWallClockTime(&is_time_moving));
  EXPECT_TRUE(is_time_moving);
}

TEST_F(AudioRendererImplTest, BitstreamEndOfStream) {
  InitializeBitstreamFormat();
  Preroll();
  StartTicking();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBitstreamBufferedData(frames_buffered()));
  WaitForPendingRead();

  // Forcefully trigger underflow.
  EXPECT_FALSE(ConsumeBitstreamBufferedData(OutputFrames(1)));
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_NOTHING, _));

  // Fulfill the read with an end-of-stream buffer. Doing so should change our
  // buffering state so playback resumes.
  EXPECT_CALL(*this, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                            BUFFERING_CHANGE_REASON_UNKNOWN));
  DeliverEndOfStream();

  // Consume all remaining data. We shouldn't have signal ended yet.
  if (frames_buffered().value != 0)
    EXPECT_TRUE(ConsumeBitstreamBufferedData(frames_buffered()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ended());

  // Ended should trigger on next render call.
  EXPECT_FALSE(ConsumeBitstreamBufferedData(OutputFrames(1)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ended());

  // Clear the use of |media_client_|, which was set in
  // InitializeBitstreamFormat().
  SetMediaClient(nullptr);
}

TEST_F(AudioRendererImplTest, SinkIsFlushed) {
  ConfigureMockRenderer(AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                        kChannelLayout, kOutputSamplesPerSecond,
                                        1024 * 15));

  Initialize();
  Preroll();
  StartTicking();
  WaitForPendingRead();
  StopTicking();

  // Start flushing.
  EXPECT_CALL(*mock_sink_, Flush());
  WaitableMessageLoopEvent flush_event;
  renderer_->Flush(flush_event.GetClosure());
  renderer_.reset();
}

}  // namespace media
