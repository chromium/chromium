// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_device_mixer_impl.h"

#include <array>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

namespace audio {
namespace {

using media::AudioOutputStream;

const std::string kDeviceId = "device-id";

MATCHER_P(AudioParamsEq, other, "AudioParameters matcher") {
  return arg.Equals(other);
}

// Mock listener.
class MockListener : public ReferenceOutput::Listener {
 public:
  MOCK_METHOD3(OnPlayoutData,
               void(const media::AudioBus& audio_bus,
                    int sample_rate,
                    base::TimeDelta delay));
};

// Mock of a physical output stream_under_test.
class MockAudioOutputStream : public AudioOutputStream {
 public:
  MockAudioOutputStream() = default;
  ~MockAudioOutputStream() override = default;
  MockAudioOutputStream(MockAudioOutputStream&& other) {
    provided_callback_ = other.provided_callback_;
  }

  void Start(AudioSourceCallback* callback) override {
    provided_callback_ = callback;
    StartCalled();
  }

  MOCK_METHOD0(StartCalled, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Open, bool());
  void SetVolume(double volume) final { volume_ = volume; }
  double GetVolume() { return volume_; }
  MOCK_METHOD1(GetVolume, void(double*));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());

  void SimulateOnMoreData() {
    DCHECK(provided_callback_);
    provided_callback_->OnMoreData(base::TimeDelta(), base::TimeTicks(), {},
                                   nullptr);
  }

  void SimulateError() {
    DCHECK(provided_callback_);
    provided_callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
  }

 private:
  raw_ptr<AudioOutputStream::AudioSourceCallback, DanglingUntriaged>
      provided_callback_ = nullptr;
  double volume_ = 0;
};

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MockAudioSourceCallback() = default;
  MockAudioSourceCallback(MockAudioSourceCallback&& other) {}

  MOCK_METHOD4(OnMoreData,
               int(base::TimeDelta delay,
                   base::TimeTicks delay_timestamp,
                   const media::AudioGlitchInfo& glitch_info,
                   media::AudioBus* dest));

  MOCK_METHOD1(OnError,
               void(AudioOutputStream::AudioSourceCallback::ErrorType type));
};

// Mocks interesting calls of MixingGraph::Input.
class MockMixingGraphInput : public MixingGraph::Input {
 public:
  MockMixingGraphInput() = default;
  MockMixingGraphInput(MockMixingGraphInput&& other) {}
  MOCK_CONST_METHOD0(GetParams, const media::AudioParameters&());
  void SetVolume(double volume) final { volume_ = volume; }
  double GetVolume() { return volume_; }
  MOCK_METHOD1(Start,
               void(AudioOutputStream::AudioSourceCallback* source_callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD3(ProvideInput,
               double(media::AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const media::AudioGlitchInfo& glitch_info));

 private:
  double volume_ = 0;
};

// MixingGraph::Input eventually produced by MockMixingGraph::CreateInput().
// Delegates all the interesting calls to MockMixingGraphInput.
class FakeMixingGraphInput : public MixingGraph::Input {
 public:
  FakeMixingGraphInput(const media::AudioParameters& params,
                       MockMixingGraphInput* mock_input)
      : params_(params), mock_input_(mock_input) {}
  const media::AudioParameters& GetParams() const final { return params_; }
  void SetVolume(double volume) final { mock_input_->SetVolume(volume); }
  void Start(AudioOutputStream::AudioSourceCallback* source_callback) final {
    mock_input_->Start(source_callback);
  }
  void Stop() final { mock_input_->Stop(); }

  MOCK_METHOD3(ProvideInput,
               double(media::AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const media::AudioGlitchInfo& glitch_info));

 private:
  const media::AudioParameters params_;
  const raw_ptr<MockMixingGraphInput> mock_input_;
};

// Created and owned by OutputMixerManagerImpl; it's essentially a factory
// producing FGraphakeMixerInput instances.
class MockMixingGraph : public MixingGraph {
 public:
  using CreateGraphInputCallback =
      base::RepeatingCallback<std::unique_ptr<MixingGraph::Input>(
          const media::AudioParameters& params)>;

  MockMixingGraph(CreateGraphInputCallback create_graph_input_cb,
                  MixingGraph::OnMoreDataCallback on_more_data_cb,
                  MixingGraph::OnErrorCallback on_error_cb)
      : create_graph_input_cb_(std::move(create_graph_input_cb)),
        on_more_data_cb_(std::move(on_more_data_cb)),
        on_error_cb_(std::move(on_error_cb)) {}

  std::unique_ptr<Input> CreateInput(
      const media::AudioParameters& params) final {
    return create_graph_input_cb_.Run(params);
  }

  MOCK_METHOD4(OnMoreData,
               int(base::TimeDelta delay,
                   base::TimeTicks delay_timestamp,
                   const media::AudioGlitchInfo& glitch_info,
                   media::AudioBus* dest));

  void OnError(AudioOutputStream::AudioSourceCallback::ErrorType type) final {
    on_error_cb_.Run(type);
  }

  void SimulateOnMoreData() {
    auto audio_bus = media::AudioBus::Create(2, 480);
    on_more_data_cb_.Run(*audio_bus.get(), base::TimeDelta());
  }

 private:
  MOCK_METHOD1(AddInput, void(MixingGraph::Input* input));
  MOCK_METHOD1(RemoveInput, void(MixingGraph::Input* input));
  CreateGraphInputCallback create_graph_input_cb_;
  MixingGraph::OnMoreDataCallback on_more_data_cb_;
  MixingGraph::OnErrorCallback on_error_cb_;
};

class OutputDeviceMixerImplTestBase {
 protected:
  enum class PlaybackMode { kMixing, kIndependent };

  // Holds all the mocks associated with a given audio output.
  struct MixTrackMock {
    explicit MixTrackMock(int frames_per_buffer)
        : params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                 media::ChannelLayoutConfig::Stereo(),
                 48000,
                 frames_per_buffer) {}
    StrictMock<MockMixingGraphInput> graph_input;
    StrictMock<MockAudioOutputStream> rendering_stream;
    const media::AudioParameters params;
    StrictMock<MockAudioSourceCallback> source_callback;
    // Set to true when independent rendering started successfully for the first
    // time.
    bool independent_rendering_stream_was_open = false;
  };

  // Helper.
  struct StreamUnderTest {
    // MixableOutputStream produced by OutputMixerImpl.
    raw_ptr<AudioOutputStream, DanglingUntriaged> mixable_stream;
    // All the mocks associated with it.
    raw_ptr<MixTrackMock> mix_track_mock;
  };

  OutputDeviceMixerImplTestBase() {
    ON_CALL(*this, MockCreateOutputStream).WillByDefault(Return(true));
  }
  // Registers physical output stream creation.
  MOCK_METHOD1(MockCreateOutputStream,
               bool(const media::AudioParameters& params));

  // Creates the mixer for testing, passing callbacks to produce mock
  // MixingGraph and mock physical output stream_under_test.
  std::unique_ptr<OutputDeviceMixer> CreateMixerUnderTest(
      const std::string& device_id = kDeviceId) {
    auto mixer = std::make_unique<OutputDeviceMixerImpl>(
        device_id, mixer_output_params_,
        base::BindOnce(&OutputDeviceMixerImplTestBase::CreateMockMixingGraph,
                       base::Unretained(this)),
        base::BindRepeating(&OutputDeviceMixerImplTestBase::CreateOutputStream,
                            base::Unretained(this), device_id));
    return mixer;
  }

  // Helper which calls the mixer to create an output stream
  // (MixabeOutputStream) and associates the result output stream with its
  // corresponding set of mocks.
  StreamUnderTest CreateNextStreamUnderTest(OutputDeviceMixer* mixer) {
    EXPECT_LE(mix_track_mocks_in_use_count_, mix_track_mocks_.size() - 1);
    const media::AudioParameters params =
        mix_track_mocks_[mix_track_mocks_in_use_count_].params;
    return {mixer->MakeMixableStream(
                params,
                base::BindOnce(
                    &OutputDeviceMixerImplTestBase::OnDeviceChangeForMixMember,
                    base::Unretained(this), params)),
            &mix_track_mocks_[mix_track_mocks_in_use_count_++]};
  }

  void ExpectIndependentRenderingStreamStreamClosedIfItWasOpen(
      StreamUnderTest& stream_under_test) {
    if (stream_under_test.mix_track_mock
            ->independent_rendering_stream_was_open) {
      EXPECT_CALL(stream_under_test.mix_track_mock->rendering_stream, Close());
    }
  }

  // Opens a MixabeOutputStream created by the mixer under test and sets
  // expectations on associated mocks.
  void OpenAndVerifyStreamUnderTest(StreamUnderTest& stream_under_test,
                                    PlaybackMode playback_mode) {
    if (playback_mode == PlaybackMode::kIndependent) {
      SetIndependentRenderingStreamOpenExpectations(
          stream_under_test.mix_track_mock, /*open_success=*/true);
    }
    EXPECT_TRUE(stream_under_test.mixable_stream->Open());
    VerifyAndClearAllExpectations();
  }

  // Closes a MixabeOutputStream created by the mixer under test and sets
  // expectations on associated mocks.
  void CloseAndVerifyStreamUnderTest(StreamUnderTest& stream_under_test) {
    ExpectIndependentRenderingStreamStreamClosedIfItWasOpen(stream_under_test);
    stream_under_test.mixable_stream->Close();
    VerifyAndClearAllExpectations();
  }

  // Starts a MixabeOutputStream created by the mixer under test and sets
  // expectations on associated mocks. |playback_mode| is the mode
  // OutputMixerImpl is expected to be in at the moment.
  void StartAndVerifyStreamUnderTest(StreamUnderTest& stream_under_test,
                                     PlaybackMode playback_mode) {
    ExpectPlaybackStarted(stream_under_test.mix_track_mock, playback_mode);
    stream_under_test.mixable_stream->Start(
        &stream_under_test.mix_track_mock->source_callback);
    VerifyAndClearAllExpectations();
  }

  // Stops a MixabeOutputStream created by the mixer under test and sets
  // expectations on associated mocks. |playback_mode| is the mode
  // OutputMixerImpl is expected to be in at the moment.
  void StopAndVerifyStreamUnderTest(StreamUnderTest& stream_under_test,
                                    PlaybackMode playback_mode) {
    ExpectPlaybackStopped(stream_under_test.mix_track_mock, playback_mode);
    stream_under_test.mixable_stream->Stop();
    VerifyAndClearAllExpectations();
  }

  // Sets expectations for the mixer to open the mixing stream.
  void ExpectMixingGraphOutputStreamOpen() {
    EXPECT_CALL(*this,
                MockCreateOutputStream(AudioParamsEq(mixer_output_params_)));
    EXPECT_CALL(mock_mixing_graph_output_stream_, Open())
        .WillOnce(Return(true));
  }

  void ExpectMixingGraphOutputStreamClosed() {
    EXPECT_CALL(mock_mixing_graph_output_stream_, Close());
  }

  // Sets expectations for the mixer to start rendering mixed audio.
  void ExpectMixingGraphOutputStreamStarted() {
    EXPECT_CALL(mock_mixing_graph_output_stream_, StartCalled());
    mixing_graph_output_stream_not_running_ = false;
  }

  // Sets expectations for the mixer to stop rendering mixed audio.
  void ExpectMixingGraphOutputStreamStopped() {
    EXPECT_CALL(mock_mixing_graph_output_stream_, Stop);
    mixing_graph_output_stream_not_running_ = true;
  }

  void SetIndependentRenderingStreamOpenExpectations(
      MixTrackMock* mix_track_mock,
      bool open_success) {
    ASSERT_FALSE(mix_track_mock->independent_rendering_stream_was_open);
    EXPECT_CALL(*this,
                MockCreateOutputStream(AudioParamsEq(mix_track_mock->params)));
    EXPECT_CALL(mix_track_mock->rendering_stream, Open())
        .WillOnce(Return(open_success));
    if (!open_success) {
      EXPECT_CALL(mix_track_mock->rendering_stream, Close());
    }
    mix_track_mock->independent_rendering_stream_was_open = open_success;
  }

  void ExpectPlaybackStarted(MixTrackMock* mix_track_mock,
                             PlaybackMode playback_mode) {
    if (playback_mode == PlaybackMode::kIndependent) {
      if (!mix_track_mock->independent_rendering_stream_was_open) {
        // If opening rendering stream during creation failed, it must be open
        // on first start.
        SetIndependentRenderingStreamOpenExpectations(mix_track_mock,
                                                      /*open_success=*/true);
      }
      EXPECT_CALL(mix_track_mock->rendering_stream, StartCalled());
    } else {
      EXPECT_CALL(mix_track_mock->graph_input,
                  Start(&mix_track_mock->source_callback));
      if (mixing_graph_output_stream_not_running_) {
        ExpectMixingGraphOutputStreamStarted();
      }
    }
  }

  void ExpectPlaybackStopped(MixTrackMock* mix_track_mock,
                             PlaybackMode playback_mode) {
    if (playback_mode == PlaybackMode::kIndependent) {
      if (mix_track_mock->independent_rendering_stream_was_open) {
        EXPECT_CALL(mix_track_mock->rendering_stream, Stop);
      }
    } else {
      EXPECT_CALL(mix_track_mock->graph_input, Stop);
    }
  }

  // Sets "start mixing" expectations for a given set of mocks.
  void ExpectMixingStarted(const std::set<MixTrackMock*>& mocks) {
    for (MixTrackMock* mock : mocks)
      EXPECT_CALL(mock->graph_input, Start(&mock->source_callback));
    if (mocks.size() && mixing_graph_output_stream_not_running_)
      ExpectMixingGraphOutputStreamStarted();
  }

  // Sets "stop mixing" expectations for a given set of mocks.
  void ExpectMixingStopped(const std::set<MixTrackMock*>& mocks) {
    for (MixTrackMock* mock : mocks)
      EXPECT_CALL(mock->graph_input, Stop);
    if (mocks.size() && !mixing_graph_output_stream_not_running_)
      ExpectMixingGraphOutputStreamStopped();
  }

  // Sets "start playing independently" expectations for a given set of mocks.
  void ExpectIndependentPlaybackStarted(const std::set<MixTrackMock*>& mocks) {
    for (MixTrackMock* mock : mocks)
      ExpectPlaybackStarted(mock, PlaybackMode::kIndependent);
  }

  // Sets "stop playing independently" expectations for a given set of mocks.
  void ExpectIndependentPlaybackStopped(const std::set<MixTrackMock*>& mocks) {
    for (MixTrackMock* mock : mocks)
      ExpectPlaybackStopped(mock, PlaybackMode::kIndependent);
  }

  // Sets "no playback change" expectations for a given set of mocks.
  void ExpectNoPlaybackModeChange(const std::set<MixTrackMock*>& mocks) {
    EXPECT_CALL(mock_mixing_graph_output_stream_, Stop).Times(0);
    EXPECT_CALL(mock_mixing_graph_output_stream_, StartCalled()).Times(0);

    for (MixTrackMock* mock : mocks) {
      EXPECT_CALL(mock->graph_input, Stop).Times(0);
      EXPECT_CALL(mock->rendering_stream, StartCalled()).Times(0);
      EXPECT_CALL(mock->graph_input, Start(_)).Times(0);
      EXPECT_CALL(mock->rendering_stream, Stop).Times(0);
    }
  }

  // Verifies and clears expectations on all mocks.
  void VerifyAndClearAllExpectations() {
    for (auto& mix_member_mock : mix_track_mocks_) {
      Mock::VerifyAndClearExpectations(&mix_member_mock.graph_input);
      Mock::VerifyAndClearExpectations(&mix_member_mock.rendering_stream);
      Mock::VerifyAndClearExpectations(&mix_member_mock.source_callback);
    }
    Mock::VerifyAndClearExpectations(&mock_mixing_graph_output_stream_);
    Mock::VerifyAndClearExpectations(this);
  }

  // Called when device change callback of a MixTrack is called.
  MOCK_METHOD1(OnDeviceChangeForMixMember,
               void(const media::AudioParameters& params));

  // Fast-forward to the point when delayed switch to unmixed playback must have
  // happened (if it was scheduled).
  void FastForwardToUnmixedPlayback() {
    task_environment_.FastForwardBy(
        OutputDeviceMixerImpl::kSwitchToUnmixedPlaybackDelay * 2);
  }

  raw_ptr<MockMixingGraph, DanglingUntriaged> mock_mixing_graph_ = nullptr;
  StrictMock<MockAudioOutputStream> mock_mixing_graph_output_stream_;

  const media::AudioParameters mixer_output_params_{
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), 48000, 5};

 private:
  AudioOutputStream* CreateOutputStream(const std::string& expected_device_id,
                                        const std::string& device_id,
                                        const media::AudioParameters& params) {
    EXPECT_EQ(device_id, expected_device_id);
    if (!MockCreateOutputStream(params))
      return nullptr;  // Fail stream creation.
    if (params.Equals(mixer_output_params_))
      return &mock_mixing_graph_output_stream_;
    return &(FindMixTrackMock(params)->rendering_stream);
  }

  // Passed into OutputMixerImpl as a callback producing MixingGraph.
  std::unique_ptr<MixingGraph> CreateMockMixingGraph(
      const media::AudioParameters& output_params,
      MixingGraph::OnMoreDataCallback on_more_data_cb,
      MixingGraph::OnErrorCallback on_error_cb) {
    auto mock_mixing_graph = std::make_unique<StrictMock<MockMixingGraph>>(
        base::BindRepeating(
            &OutputDeviceMixerImplTestBase::CreateMockGraphInput,
            base::Unretained(this)),
        std::move(on_more_data_cb), std::move(on_error_cb));
    mock_mixing_graph_ = mock_mixing_graph.get();
    return mock_mixing_graph;
  }

  // Passed into MockMixingGraph as a callback producing MixingGraph::Input
  // instances.
  std::unique_ptr<MixingGraph::Input> CreateMockGraphInput(
      const media::AudioParameters& params) {
    return std::make_unique<FakeMixingGraphInput>(
        params, &(FindMixTrackMock(params)->graph_input));
  }

  // Finds the mocks for MixableOutputStream created with given |params|.
  MixTrackMock* FindMixTrackMock(const media::AudioParameters& params) {
    for (auto& track_mock : mix_track_mocks_) {
      if (track_mock.params.Equals(params))
        return &track_mock;
    }
    DCHECK(false);
    return nullptr;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  // A set of mocks for the data associated with MixableOutputStream; each mock
  // is identfied by its MixTrackMock::|params|.frames_per_buffer().
  // To create mixable_stream output streams_under_test the tests call
  // OutputDeviceMixerImpl::MakeMixableStream with MixTrackMock::|params|
  // parameters, thus building an association between a MixableOutputStream and
  // MixTrackMock.
  std::array<MixTrackMock, 3> mix_track_mocks_{
      MixTrackMock(10), MixTrackMock(20), MixTrackMock(30)};

  // How many elements of |mix_track_mocks_| are already in use.
  size_t mix_track_mocks_in_use_count_ = 0;

  // Helps to correctly set expectations for |mock_mixing_graph_output_stream_|.
  bool mixing_graph_output_stream_not_running_ = true;
};

class OutputDeviceMixerImplTest : public OutputDeviceMixerImplTestBase,
                                  public testing::TestWithParam<int> {};

class OutputDeviceMixerImplTestWithDefault
    : public OutputDeviceMixerImplTestBase,
      public testing::TestWithParam<std::string> {};

TEST_P(OutputDeviceMixerImplTestWithDefault, OneUmixedStream_CreateClose) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest(GetParam());
  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());

  // Physical output streams are created only on Open().
  EXPECT_CALL(*this, MockCreateOutputStream(_)).Times(0);

  EXPECT_CALL(stream_under_test.mix_track_mock->rendering_stream, Open())
      .Times(0);
  EXPECT_CALL(mock_mixing_graph_output_stream_, Open()).Times(0);
  EXPECT_CALL(stream_under_test.mix_track_mock->rendering_stream, Close())
      .Times(0);
  EXPECT_CALL(mock_mixing_graph_output_stream_, Close()).Times(0);

  CloseAndVerifyStreamUnderTest(stream_under_test);
}

TEST_F(OutputDeviceMixerImplTest, OneUmixedStream_PhysicalStreamCreateFailed) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();
  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());

  // Fail physical stream creation for the mix member. Open() fails, physical
  // stream for independent rendering is not created.
  EXPECT_CALL(*this, MockCreateOutputStream(AudioParamsEq(
                         stream_under_test.mix_track_mock->params)))
      .WillOnce(Return(false));
  EXPECT_CALL(stream_under_test.mix_track_mock->rendering_stream, Open())
      .Times(0);
  EXPECT_CALL(stream_under_test.mix_track_mock->rendering_stream, Close())
      .Times(0);

  EXPECT_CALL(*this,
              MockCreateOutputStream(AudioParamsEq(mixer_output_params_)))
      .Times(0);
  EXPECT_CALL(mock_mixing_graph_output_stream_, Open()).Times(0);
  EXPECT_CALL(mock_mixing_graph_output_stream_, Close()).Times(0);

  EXPECT_FALSE(stream_under_test.mixable_stream->Open());
  CloseAndVerifyStreamUnderTest(stream_under_test);
}

TEST_F(OutputDeviceMixerImplTest, OneUmixedStream_PhysicalStreamOpenFailed) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();
  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());

  // Fail opening physical stream for the mix track. Open() fails, physical
  // stream for independent rendering is not created.
  SetIndependentRenderingStreamOpenExpectations(
      stream_under_test.mix_track_mock,
      /*open_success=*/false);

  EXPECT_CALL(mock_mixing_graph_output_stream_, Open()).Times(0);
  EXPECT_CALL(mock_mixing_graph_output_stream_, Close()).Times(0);

  EXPECT_FALSE(stream_under_test.mixable_stream->Open());
  CloseAndVerifyStreamUnderTest(stream_under_test);
}

TEST_P(OutputDeviceMixerImplTestWithDefault, OneUmixedStream_CreateOpenClose) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest(GetParam());
  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  CloseAndVerifyStreamUnderTest(stream_under_test);
}

TEST_F(OutputDeviceMixerImplTest, TwoUmixedStream_CreateOpenClose) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();
  StreamUnderTest stream_under_test1 = CreateNextStreamUnderTest(mixer.get());
  StreamUnderTest stream_under_test2 = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test1, PlaybackMode::kIndependent);
  OpenAndVerifyStreamUnderTest(stream_under_test2, PlaybackMode::kIndependent);
  CloseAndVerifyStreamUnderTest(stream_under_test2);
  CloseAndVerifyStreamUnderTest(stream_under_test1);
}

TEST_F(OutputDeviceMixerImplTest,
       TwoUmixedStream_SecondOpenFailureDoesNotAffectMixerStream) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();
  StreamUnderTest stream_under_test1 = CreateNextStreamUnderTest(mixer.get());
  StreamUnderTest stream_under_test2 = CreateNextStreamUnderTest(mixer.get());

  OpenAndVerifyStreamUnderTest(stream_under_test1, PlaybackMode::kIndependent);
  StartAndVerifyStreamUnderTest(stream_under_test1, PlaybackMode::kIndependent);

  // Failing to open stream_under_test2 does not affect stream_under_test2.
  SetIndependentRenderingStreamOpenExpectations(
      stream_under_test2.mix_track_mock, /*open_success=*/false);
  EXPECT_FALSE(stream_under_test2.mixable_stream->Open());
  VerifyAndClearAllExpectations();
  CloseAndVerifyStreamUnderTest(stream_under_test2);

  StopAndVerifyStreamUnderTest(stream_under_test1, PlaybackMode::kIndependent);
  CloseAndVerifyStreamUnderTest(stream_under_test1);
}

TEST_F(OutputDeviceMixerImplTest, OneUmixedStream_SetVolumeIsPropagated) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();
  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());

  double volume1 = 0.1;
  double volume2 = 0.2;
  double volume3 = 0.3;
  double volume4 = 0.3;
  double volume_result = 0;

  // Volume is propagated before MixableOutputStream::Open().
  stream_under_test.mixable_stream->SetVolume(volume1);
  stream_under_test.mixable_stream->GetVolume(&volume_result);
  EXPECT_EQ(volume_result, volume1);
  EXPECT_EQ(stream_under_test.mix_track_mock->graph_input.GetVolume(), volume1);

  stream_under_test.mixable_stream->SetVolume(volume2);
  stream_under_test.mixable_stream->GetVolume(&volume_result);
  EXPECT_EQ(volume_result, volume2);
  EXPECT_EQ(stream_under_test.mix_track_mock->graph_input.GetVolume(), volume2);

  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);

  // Volume is propagated after MixableOutputStream::Open().
  stream_under_test.mixable_stream->GetVolume(&volume_result);
  EXPECT_EQ(volume_result, volume2);
  EXPECT_EQ(stream_under_test.mix_track_mock->graph_input.GetVolume(), volume2);

  StartAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);

  // Volume is applied to the independent rendering stream of the MixTrack.
  EXPECT_EQ(stream_under_test.mix_track_mock->rendering_stream.GetVolume(),
            volume2);

  // Now when the stream is open, the volume is propagated to both the mixer
  // input and the independent rendering tream of the MixTrack.
  stream_under_test.mixable_stream->SetVolume(volume3);
  stream_under_test.mixable_stream->GetVolume(&volume_result);
  EXPECT_EQ(volume_result, volume3);
  EXPECT_EQ(stream_under_test.mix_track_mock->graph_input.GetVolume(), volume3);
  EXPECT_EQ(stream_under_test.mix_track_mock->rendering_stream.GetVolume(),
            volume3);

  StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);

  stream_under_test.mixable_stream->SetVolume(volume4);
  stream_under_test.mixable_stream->GetVolume(&volume_result);
  EXPECT_EQ(volume_result, volume4);
  EXPECT_EQ(stream_under_test.mix_track_mock->graph_input.GetVolume(), volume4);
  EXPECT_EQ(stream_under_test.mix_track_mock->rendering_stream.GetVolume(),
            volume4);

  CloseAndVerifyStreamUnderTest(stream_under_test);
}

TEST_F(OutputDeviceMixerImplTest,
       OneUmixedStream_CreateOpenPlayOnErrorStopClose) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();
  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());

  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);

  {
    InSequence s;

    StartAndVerifyStreamUnderTest(stream_under_test,
                                  PlaybackMode::kIndependent);

    EXPECT_CALL(stream_under_test.mix_track_mock->source_callback,
                OnMoreData(_, _, _, _))
        .Times(1);
    EXPECT_CALL(stream_under_test.mix_track_mock->source_callback, OnError(_))
        .Times(1);

    stream_under_test.mix_track_mock->rendering_stream.SimulateOnMoreData();
    stream_under_test.mix_track_mock->rendering_stream.SimulateError();

    VerifyAndClearAllExpectations();

    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  }

  // Make sure it works after restart as well.
  {
    InSequence s;

    StartAndVerifyStreamUnderTest(stream_under_test,
                                  PlaybackMode::kIndependent);

    EXPECT_CALL(stream_under_test.mix_track_mock->source_callback,
                OnMoreData(_, _, _, _))
        .Times(1);
    EXPECT_CALL(stream_under_test.mix_track_mock->source_callback, OnError(_))
        .Times(1);

    stream_under_test.mix_track_mock->rendering_stream.SimulateOnMoreData();
    stream_under_test.mix_track_mock->rendering_stream.SimulateError();

    VerifyAndClearAllExpectations();

    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  }

  CloseAndVerifyStreamUnderTest(stream_under_test);
}

TEST_P(OutputDeviceMixerImplTest,
       NStreamsOpen_StartStopListeningDoesNotStartMixing) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  int stream_count = GetParam();
  std::vector<StreamUnderTest> streams_under_test;
  std::set<MixTrackMock*> playing_stream_mocks;
  for (int i = 0; i < stream_count; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kIndependent);
  }

  // The mixer may have streams open, but they are not playing, so
  // Start/StopListening has no effect.
  MockListener listener;
  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  ExpectMixingGraphOutputStreamClosed();
  mixer->StopListening(&listener);
  VerifyAndClearAllExpectations();

  FastForwardToUnmixedPlayback();
  for (auto& stream_under_test : streams_under_test) {
    CloseAndVerifyStreamUnderTest(stream_under_test);
  }
}

TEST_P(OutputDeviceMixerImplTest, NStreamsPlaying_StartStopTwoListeners) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  int stream_count = GetParam();
  if (!stream_count)
    return;  // Not interesting.
  std::vector<StreamUnderTest> streams_under_test;
  std::set<MixTrackMock*> playing_stream_mocks;
  for (int i = 0; i < stream_count; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kIndependent);
    StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                  PlaybackMode::kIndependent);
    playing_stream_mocks.insert((&streams_under_test.back())->mix_track_mock);
  }

  MockListener listener1;
  MockListener listener2;

  // We are playing at least one stream.
  // Expect switch to mixing when the first listener comes.
  ExpectIndependentPlaybackStopped(playing_stream_mocks);
  ExpectMixingGraphOutputStreamOpen();
  ExpectMixingStarted(playing_stream_mocks);
  mixer->StartListening(&listener1);
  FastForwardToUnmixedPlayback();
  VerifyAndClearAllExpectations();

  // Nothing should change when the second listener comes.
  ExpectNoPlaybackModeChange(playing_stream_mocks);
  mixer->StartListening(&listener2);
  FastForwardToUnmixedPlayback();
  VerifyAndClearAllExpectations();

  // First listener leaves: we still should be mixing.
  ExpectNoPlaybackModeChange(playing_stream_mocks);
  mixer->StopListening(&listener1);
  FastForwardToUnmixedPlayback();
  VerifyAndClearAllExpectations();

  // Second listener leaves: switching to unmixed playback is delayed, so we do
  // not expect it to happen immediately.
  ExpectNoPlaybackModeChange(playing_stream_mocks);
  mixer->StopListening(&listener2);
  VerifyAndClearAllExpectations();

  // Expect switching to unmixed playback after we fast-forward to the future.
  ExpectMixingGraphOutputStreamClosed();
  ExpectMixingStopped(playing_stream_mocks);
  ExpectIndependentPlaybackStarted(playing_stream_mocks);
  FastForwardToUnmixedPlayback();
  VerifyAndClearAllExpectations();

  for (auto& stream_under_test : streams_under_test) {
    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
    CloseAndVerifyStreamUnderTest(stream_under_test);
  }
}

TEST_P(OutputDeviceMixerImplTest,
       NStreamsPlaying_StopStartListening_DoesNotStopMixing) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  int stream_count = GetParam();
  if (!stream_count)
    return;  // Not interesting.
  std::vector<StreamUnderTest> streams_under_test;
  std::set<MixTrackMock*> playing_stream_mocks;
  for (int i = 0; i < stream_count; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kIndependent);
    StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                  PlaybackMode::kIndependent);
    playing_stream_mocks.insert((&streams_under_test.back())->mix_track_mock);
  }

  MockListener listener;

  // We are playing at least one stream.
  // Expect switch to mixing when the listener comes.
  ExpectIndependentPlaybackStopped(playing_stream_mocks);
  ExpectMixingGraphOutputStreamOpen();
  ExpectMixingStarted(playing_stream_mocks);
  mixer->StartListening(&listener);
  FastForwardToUnmixedPlayback();
  VerifyAndClearAllExpectations();

  // If the listener stops listening and then starts again immediately, we do
  // not switch to unmixed playback even after the timeout.
  ExpectNoPlaybackModeChange(playing_stream_mocks);
  mixer->StopListening(&listener);
  mixer->StartListening(&listener);
  FastForwardToUnmixedPlayback();
  VerifyAndClearAllExpectations();

  // Stop listening and expect the switch to unmixed playback upon the timeout.
  ExpectMixingStopped(playing_stream_mocks);
  ExpectMixingGraphOutputStreamClosed();
  ExpectIndependentPlaybackStarted(playing_stream_mocks);
  mixer->StopListening(&listener);
  FastForwardToUnmixedPlayback();
  VerifyAndClearAllExpectations();

  for (auto& stream_under_test : streams_under_test) {
    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
    CloseAndVerifyStreamUnderTest(stream_under_test);
  }
}

TEST_P(OutputDeviceMixerImplTest,
       StartStopNStreamsWhileListening_MixedPlaybackUntilListenerGone) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  int stream_count = GetParam();
  if (!stream_count)
    return;  // Not interesting.

  MockListener listener;
  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  std::vector<StreamUnderTest> streams_under_test;
  for (int i = 0; i < stream_count; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kMixing);
    StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                  PlaybackMode::kMixing);
  }

  // Stopping all playback does not stop rendering of the mixing graph.
  for (auto& stream_under_test : streams_under_test) {
    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
    FastForwardToUnmixedPlayback();
    VerifyAndClearAllExpectations();
  }

  // Restarting playback.
  for (auto& stream_under_test : streams_under_test) {
    StartAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
    FastForwardToUnmixedPlayback();
    VerifyAndClearAllExpectations();
  }

  // Stopping and closing all the streams does not stop rendering of the mixing
  // graph.
  for (auto& stream_under_test : streams_under_test) {
    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
    CloseAndVerifyStreamUnderTest(stream_under_test);
  }

  FastForwardToUnmixedPlayback();
  VerifyAndClearAllExpectations();

  // Now when we stop the listener, rendering of the mixing graph should
  // stop immediately. Mixing graph output stream will be closed.
  ExpectMixingGraphOutputStreamStopped();
  ExpectMixingGraphOutputStreamClosed();
  mixer->StopListening(&listener);
  VerifyAndClearAllExpectations();
}

TEST_P(OutputDeviceMixerImplTest, StartStopNStreamsWhileListening_DeleteMixer) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  MockListener listener;
  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  int stream_count = GetParam();
  std::vector<StreamUnderTest> streams_under_test;
  for (int i = 0; i < stream_count; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kMixing);
    StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                  PlaybackMode::kMixing);
  }

  for (auto& stream_under_test : streams_under_test) {
    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
    CloseAndVerifyStreamUnderTest(stream_under_test);
  }

  if (stream_count)
    ExpectMixingGraphOutputStreamStopped();
  ExpectMixingGraphOutputStreamClosed();
  mixer->StopListening(&listener);
  mixer = nullptr;
  // Mixer output stream must be closed immediately.
  VerifyAndClearAllExpectations();
}

TEST_F(OutputDeviceMixerImplTest,
       DeleteMixer_WhileGraphOutputStreamStopIsDelayed) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  MockListener listener;
  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  auto stream_under_test = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
  StartAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
  mixer->StopListening(&listener);

  // Since there are no listeners left, mixing playback must be stopped as soon
  // as the stream is gone.
  ExpectMixingGraphOutputStreamStopped();
  ExpectMixingGraphOutputStreamClosed();
  StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);

  CloseAndVerifyStreamUnderTest(stream_under_test);

  mixer = nullptr;
  VerifyAndClearAllExpectations();
}

TEST_P(OutputDeviceMixerImplTest, NStreamsMixing_OnMixingStreamError) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  int stream_count = GetParam();
  if (!stream_count)
    return;  // Not interesting.

  MockListener listener;
  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  std::vector<StreamUnderTest> streams_under_test;
  std::set<MixTrackMock*> playing_stream_mocks;
  for (int i = 0; i < stream_count; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kMixing);
    StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                  PlaybackMode::kMixing);
    playing_stream_mocks.insert((&streams_under_test.back())->mix_track_mock);
  }

  for (auto& stream_under_test : streams_under_test)
    EXPECT_CALL(stream_under_test.mix_track_mock->source_callback, OnError(_));

  ExpectMixingStopped(playing_stream_mocks);
  ExpectMixingGraphOutputStreamClosed();  // To be able to recover in the
                                          // future.
  mock_mixing_graph_output_stream_.SimulateError();
  VerifyAndClearAllExpectations();

  for (auto& stream_under_test : streams_under_test) {
    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  }

  mixer->StopListening(&listener);
  for (auto& stream_under_test : streams_under_test) {
    CloseAndVerifyStreamUnderTest(stream_under_test);
  }
  mixer.reset();
  VerifyAndClearAllExpectations();
}

TEST_F(OutputDeviceMixerImplTest, OnMixingStreamError_Recovers) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  MockListener listener;
  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  std::vector<StreamUnderTest> streams_under_test;
  std::set<MixTrackMock*> playing_stream_mocks;
  for (int i = 0; i < 2; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kMixing);
    StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                  PlaybackMode::kMixing);
    playing_stream_mocks.insert((&streams_under_test.back())->mix_track_mock);
  }

  // Simulate the mixing stream error.
  for (auto& stream_under_test : streams_under_test)
    EXPECT_CALL(stream_under_test.mix_track_mock->source_callback, OnError(_));
  ExpectMixingStopped(playing_stream_mocks);
  ExpectMixingGraphOutputStreamClosed();
  mock_mixing_graph_output_stream_.SimulateError();
  VerifyAndClearAllExpectations();

  // Opening and starting a new stream: mixing should retry and start
  // successfully.
  streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
  OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                               PlaybackMode::kMixing);

  // Mixing should restart now.
  ExpectMixingGraphOutputStreamOpen();
  ExpectMixingStarted(playing_stream_mocks);
  StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                PlaybackMode::kMixing);
  VerifyAndClearAllExpectations();

  for (auto& stream_under_test : streams_under_test) {
    StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
  }

  // No playback, the listener is gone - we should stop mixing.
  ExpectMixingGraphOutputStreamStopped();
  ExpectMixingGraphOutputStreamClosed();
  mixer->StopListening(&listener);
  VerifyAndClearAllExpectations();

  for (auto& stream_under_test : streams_under_test) {
    CloseAndVerifyStreamUnderTest(stream_under_test);
  }
}

TEST_P(OutputDeviceMixerImplTest, NStreamsPlayingUmixed_DeviceChange) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  int stream_count = GetParam();

  std::vector<StreamUnderTest> streams_under_test;

  for (int i = 0; i < stream_count; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kIndependent);
    if (i) {  // Leave one stream just open.
      StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                    PlaybackMode::kIndependent);
    }
  }

  for (int i = 0; i < stream_count; ++i) {
    if (i) {
      EXPECT_CALL(streams_under_test[i].mix_track_mock->rendering_stream, Stop);
    }
    ExpectIndependentRenderingStreamStreamClosedIfItWasOpen(
        streams_under_test[i]);
    EXPECT_CALL(*this, OnDeviceChangeForMixMember(AudioParamsEq(
                           streams_under_test[i].mix_track_mock->params)));
  }

  mixer->ProcessDeviceChange();
  VerifyAndClearAllExpectations();

  // Stop() and Close() operations on streams are no-op now.
  for (int i = 0; i < stream_count; ++i) {
    if (i) {
      streams_under_test[i].mixable_stream->Stop();
    }
    streams_under_test[i].mixable_stream->Close();
  }
}

TEST_P(OutputDeviceMixerImplTest, NStreamsPlayingMixed_DeviceChange) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  int stream_count = GetParam();

  MockListener listener;
  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  std::vector<StreamUnderTest> streams_under_test;
  for (int i = 0; i < stream_count; ++i) {
    streams_under_test.push_back(CreateNextStreamUnderTest(mixer.get()));
    OpenAndVerifyStreamUnderTest(streams_under_test.back(),
                                 PlaybackMode::kMixing);
    if (i) {  // Leave one stream just open.
      StartAndVerifyStreamUnderTest(streams_under_test.back(),
                                    PlaybackMode::kMixing);
    }
  }

  for (int i = 0; i < stream_count; ++i) {
    if (i) {
      EXPECT_CALL(streams_under_test[i].mix_track_mock->graph_input, Stop);
    }
    ExpectIndependentRenderingStreamStreamClosedIfItWasOpen(
        streams_under_test[i]);
    EXPECT_CALL(*this, OnDeviceChangeForMixMember(AudioParamsEq(
                           streams_under_test[i].mix_track_mock->params)));
  }

  if (stream_count > 1)  // Since we do not start the first stream.
    ExpectMixingGraphOutputStreamStopped();
  ExpectMixingGraphOutputStreamClosed();

  mixer->ProcessDeviceChange();
  VerifyAndClearAllExpectations();

  // Stop() and Close() operations on streams are no-op now.
  for (int i = 0; i < stream_count; ++i) {
    if (i) {
      streams_under_test[i].mixable_stream->Stop();
    }
    streams_under_test[i].mixable_stream->Close();
  }
}

TEST_F(OutputDeviceMixerImplTest, OnMoreDataDeliversCallbacks) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  MockListener listener1;
  MockListener listener2;

  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener1);
  VerifyAndClearAllExpectations();

  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
  StartAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);

  EXPECT_CALL(listener1, OnPlayoutData(_, _, _)).Times(1);
  mock_mixing_graph_->SimulateOnMoreData();
  VerifyAndClearAllExpectations();

  mixer->StartListening(&listener2);

  EXPECT_CALL(listener1, OnPlayoutData(_, _, _)).Times(1);
  EXPECT_CALL(listener2, OnPlayoutData(_, _, _)).Times(1);
  mock_mixing_graph_->SimulateOnMoreData();
  VerifyAndClearAllExpectations();

  mixer->StopListening(&listener1);

  EXPECT_CALL(listener2, OnPlayoutData(_, _, _)).Times(1);
  mock_mixing_graph_->SimulateOnMoreData();
  VerifyAndClearAllExpectations();

  StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);
  CloseAndVerifyStreamUnderTest(stream_under_test);

  ExpectMixingGraphOutputStreamStopped();
  ExpectMixingGraphOutputStreamClosed();
  mixer->StopListening(&listener2);
}

TEST_F(OutputDeviceMixerImplTest,
       MixingStreamCreationFailureHandledOnMixingPlaybackStart) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();
  MockListener listener;
  // Fail creating the mixing stream.
  EXPECT_CALL(*this,
              MockCreateOutputStream(AudioParamsEq(mixer_output_params_)))
      .WillOnce(Return(false));
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);

  // Since the previous attempt to create the mixing stream failed, it will
  // retry now when starting mixing playback. Fail it again.
  EXPECT_CALL(*this,
              MockCreateOutputStream(AudioParamsEq(mixer_output_params_)))
      .WillOnce(Return(false));

  EXPECT_CALL(stream_under_test.mix_track_mock->source_callback, OnError(_));

  stream_under_test.mixable_stream->Start(
      &stream_under_test.mix_track_mock->source_callback);

  VerifyAndClearAllExpectations();

  // Since mixing has not started, the mixer considers it as independent
  // playback and will stop the independent rendering stream. It's ok:
  // media::AudioOutputStream can be closed multiple times.
  StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  CloseAndVerifyStreamUnderTest(stream_under_test);

  mixer->StopListening(&listener);
}

TEST_F(OutputDeviceMixerImplTest,
       MixingStreamOpenFailureHandled_ListenerPresent) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  MockListener listener;
  // Fail opening the mixing stream.
  EXPECT_CALL(*this,
              MockCreateOutputStream(AudioParamsEq(mixer_output_params_)));
  EXPECT_CALL(mock_mixing_graph_output_stream_, Open()).WillOnce(Return(false));
  EXPECT_CALL(mock_mixing_graph_output_stream_, Close());
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);

  // Since the previous attempt to create the mixing stream failed, it will
  // retry now when starting mixing playback. Fail it again.
  EXPECT_CALL(*this,
              MockCreateOutputStream(AudioParamsEq(mixer_output_params_)));
  EXPECT_CALL(mock_mixing_graph_output_stream_, Open()).WillOnce(Return(false));
  EXPECT_CALL(mock_mixing_graph_output_stream_, Close());

  EXPECT_CALL(stream_under_test.mix_track_mock->source_callback, OnError(_));

  stream_under_test.mixable_stream->Start(
      &stream_under_test.mix_track_mock->source_callback);

  VerifyAndClearAllExpectations();

  // Since mixing has not started, the mixer considers it as independent
  // playback and will stop the independent rendering stream. It's ok:
  // media::AudioOutputStream can be closed multiple times.
  StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  CloseAndVerifyStreamUnderTest(stream_under_test);

  mixer->StopListening(&listener);
}

TEST_F(OutputDeviceMixerImplTest,
       MixingStreamOpenFailureHandled_WhenStartListening) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  MockListener listener;

  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  StartAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);

  EXPECT_CALL(*this,
              MockCreateOutputStream(AudioParamsEq(mixer_output_params_)));
  EXPECT_CALL(mock_mixing_graph_output_stream_, Open()).WillOnce(Return(false));
  EXPECT_CALL(mock_mixing_graph_output_stream_, Close());
  EXPECT_CALL(stream_under_test.mix_track_mock->rendering_stream, Stop);
  EXPECT_CALL(stream_under_test.mix_track_mock->source_callback, OnError(_));

  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  // Since mixing has not started, the mixer considers it as independent
  // playback and will stop the independent rendering stream. It's ok:
  // media::AudioOutputStream can be closed multiple times.
  StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  CloseAndVerifyStreamUnderTest(stream_under_test);

  mixer->StopListening(&listener);
}

TEST_F(OutputDeviceMixerImplTest,
       MixingStreamCreationFailureHandled_NextMixingSuccessful) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  MockListener listener;
  // Fail creating the mixing stream.
  EXPECT_CALL(*this,
              MockCreateOutputStream(AudioParamsEq(mixer_output_params_)))
      .WillOnce(Return(false));
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  StreamUnderTest stream_under_test1 = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test1, PlaybackMode::kMixing);

  // Fail creating the mixing stream again.
  EXPECT_CALL(*this,
              MockCreateOutputStream(AudioParamsEq(mixer_output_params_)))
      .WillOnce(Return(false));
  EXPECT_CALL(stream_under_test1.mix_track_mock->source_callback, OnError(_));

  stream_under_test1.mixable_stream->Start(
      &stream_under_test1.mix_track_mock->source_callback);
  VerifyAndClearAllExpectations();

  // Since mixing has not started, the mixer considers it as independent
  // playback and will stop the independent rendering stream. It's ok:
  // media::AudioOutputStream can be closed multiple times.
  StopAndVerifyStreamUnderTest(stream_under_test1, PlaybackMode::kIndependent);
  CloseAndVerifyStreamUnderTest(stream_under_test1);

  // This time mixing should be successful.
  StreamUnderTest stream_under_test2 = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test2, PlaybackMode::kMixing);

  ExpectMixingGraphOutputStreamOpen();
  StartAndVerifyStreamUnderTest(stream_under_test2, PlaybackMode::kMixing);
  StopAndVerifyStreamUnderTest(stream_under_test2, PlaybackMode::kMixing);
  CloseAndVerifyStreamUnderTest(stream_under_test2);

  ExpectMixingGraphOutputStreamStopped();
  ExpectMixingGraphOutputStreamClosed();
  mixer->StopListening(&listener);
}

TEST_F(OutputDeviceMixerImplTest,
       StartListening_OpenStream_StopListening_StartIndependentPlayback) {
  std::unique_ptr<OutputDeviceMixer> mixer = CreateMixerUnderTest();

  MockListener listener;
  ExpectMixingGraphOutputStreamOpen();
  mixer->StartListening(&listener);
  VerifyAndClearAllExpectations();

  StreamUnderTest stream_under_test = CreateNextStreamUnderTest(mixer.get());
  OpenAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kMixing);

  ExpectMixingGraphOutputStreamClosed();
  mixer->StopListening(&listener);
  VerifyAndClearAllExpectations();

  // Since there were listeners attached when |stream_under_test| was open, its
  // physical rendering stream is not open yet.
  SetIndependentRenderingStreamOpenExpectations(
      stream_under_test.mix_track_mock,
      /*open_success=*/true);
  StartAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  StopAndVerifyStreamUnderTest(stream_under_test, PlaybackMode::kIndependent);
  CloseAndVerifyStreamUnderTest(stream_under_test);
}

INSTANTIATE_TEST_SUITE_P(,
                         OutputDeviceMixerImplTest,
                         testing::Values(0, 1, 2, 3));

INSTANTIATE_TEST_SUITE_P(,
                         OutputDeviceMixerImplTestWithDefault,
                         testing::Values(kDeviceId, ""));

}  // namespace

}  // namespace audio
