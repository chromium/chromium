// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_device_mixer_manager.h"

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "services/audio/output_device_mixer.h"
#include "services/audio/reference_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

using media::AudioOutputStream;
using media::AudioParameters;

using base::test::RunOnceClosure;

namespace audio {
namespace {

// Matches non-null device change callbacks.
MATCHER(ValidDeviceChangeCallback, "") {
  return !arg.is_null();
}

// Matches an expected media::AudioParameters.
MATCHER_P(ExactParams, expected, "") {
  return expected.Equals(arg);
}

// Matches media::AudioParameters that are equal in all aspects,
// except for sample_per_buffer()
MATCHER_P(CompatibleParams, expected, "") {
  return expected.format() == arg.format() &&
         expected.channel_layout() == arg.channel_layout() &&
         expected.channels() == arg.channels() &&
         expected.effects() == arg.effects() &&
         expected.mic_positions() == arg.mic_positions() &&
         expected.latency_tag() == arg.latency_tag();
}

const std::string kFakeDeviceId = "0x1234";
const std::string kOtherFakeDeviceId = "0x9876";
const std::string kDefaultDeviceId = "default";
const std::string kEmptyDeviceId = "";

class MockAudioOutputStream : public AudioOutputStream {
 public:
  MockAudioOutputStream() = default;
  ~MockAudioOutputStream() override = default;

  MOCK_METHOD1(Start, void(AudioOutputStream::AudioSourceCallback*));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());
};

class LocalMockAudioManager : public media::MockAudioManager {
 public:
  LocalMockAudioManager()
      : media::MockAudioManager(
            std::make_unique<media::TestAudioThread>(false)) {}
  ~LocalMockAudioManager() override = default;

  MOCK_METHOD(std::string, GetDefaultOutputDeviceID, (), (override));
  MOCK_METHOD(AudioParameters,
              GetOutputStreamParameters,
              (const std::string&),
              (override));

  MOCK_METHOD(AudioOutputStream*,
              MakeAudioOutputStreamProxy,
              (const media::AudioParameters&, const std::string&),
              (override));
};

class MockListener : public audio::ReferenceOutput::Listener {
 public:
  MockListener() = default;
  ~MockListener() override = default;

  MOCK_METHOD(void,
              OnPlayoutData,
              (const media::AudioBus&, int, base::TimeDelta),
              (override));
};

class MockOutputDeviceMixer : public audio::OutputDeviceMixer {
 public:
  explicit MockOutputDeviceMixer(const std::string& device_id)
      : OutputDeviceMixer(device_id) {}
  ~MockOutputDeviceMixer() override = default;

  MOCK_METHOD(media::AudioOutputStream*,
              MakeMixableStream,
              (const AudioParameters&, base::OnceClosure),
              (override));
  MOCK_METHOD(void, ProcessDeviceChange, (), (override));

  MOCK_METHOD(void, StartListening, (Listener*), (override));
  MOCK_METHOD(void, StopListening, (Listener*), (override));
};
}  // namespace

class OutputDeviceMixerManagerTest
    : public ::testing::TestWithParam<std::string> {
 public:
  OutputDeviceMixerManagerTest()
      : current_default_device_id_(kFakeDeviceId),
        default_params_(AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
                        media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                        /*sample_rate=*/8000,
                        /*frames_per_buffer=*/800),
        output_mixer_manager_(
            &audio_manager_,
            base::BindRepeating(
                &OutputDeviceMixerManagerTest::CreateOutputDeviceMixerCalled,
                base::Unretained(this))) {
    EXPECT_CALL(audio_manager_, GetOutputStreamParameters(_))
        .WillRepeatedly(Return(default_params_));

    EXPECT_CALL(audio_manager_, GetDefaultOutputDeviceID()).WillRepeatedly([&] {
      return current_default_device_id_;
    });
  }

  ~OutputDeviceMixerManagerTest() override { audio_manager_.Shutdown(); }

  MOCK_METHOD(std::unique_ptr<OutputDeviceMixer>,
              CreateOutputDeviceMixerCalled,
              (const std::string&,
               const media::AudioParameters&,
               OutputDeviceMixer::CreateStreamCallback,
               scoped_refptr<base::SingleThreadTaskRunner>));

 protected:
  MockOutputDeviceMixer* SetUpMockMixerCreation(std::string device_id) {
    auto mock_output_mixer =
        std::make_unique<NiceMock<MockOutputDeviceMixer>>(device_id);
    MockOutputDeviceMixer* mixer = mock_output_mixer.get();

    EXPECT_CALL(*this, CreateOutputDeviceMixerCalled(
                           device_id, CompatibleParams(default_params_), _, _))
        .WillOnce(Return(ByMove(std::move(mock_output_mixer))));

    return mixer;
  }

  // Sets up a mock OutputDeviceMixer for creation, which will only return
  // nullptr when creating streams.
  MockOutputDeviceMixer* SetUpMockMixer_NoStreams(std::string device_id) {
    MockOutputDeviceMixer* output_mixer = SetUpMockMixerCreation(device_id);

    EXPECT_CALL(*output_mixer, MakeMixableStream(_, _))
        .WillRepeatedly(Return(nullptr));

    return output_mixer;
  }

  std::unique_ptr<NiceMock<MockListener>> GetListenerWithStartStopExpectations(
      MockOutputDeviceMixer* mixer,
      int starts,
      int stops) {
    auto listener = std::make_unique<NiceMock<MockListener>>();

    auto* listener_ptr = listener.get();

    EXPECT_CALL(*mixer, StartListening(listener_ptr)).Times(starts);
    EXPECT_CALL(*mixer, StopListening(listener_ptr)).Times(stops);

    return listener;
  }

  void ForceOutputMixerCreation(const std::string& device_id) {
    output_mixer_manager_.MakeOutputStream(device_id, default_params_,
                                           GetNoopDeviceChangeCallback());
  }

  void SimulateDeviceChange(
      const std::string& new_default_device_id = kFakeDeviceId) {
    current_default_device_id_ = new_default_device_id;
    output_mixer_manager_.OnDeviceChange();
  }

  void ExpectNoMixerCreated() {
    EXPECT_CALL(*this, CreateOutputDeviceMixerCalled(_, _, _, _)).Times(0);
  }

  // Syntactic sugar, to differentiate from base::OnceClosure in tests.
  base::OnceClosure GetNoopDeviceChangeCallback() { return base::DoNothing(); }

  std::string current_default_device_id_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  AudioParameters default_params_;
  NiceMock<LocalMockAudioManager> audio_manager_;
  OutputDeviceMixerManager output_mixer_manager_;
};

// Used for tests that deal with the default output device, which can be
// represented as kDefaultDeviceId or kEmptyDeviceId.
class OutputDeviceMixerManagerTestWithDefault
    : public OutputDeviceMixerManagerTest {
 protected:
  std::string default_device_id() { return GetParam(); }
};

// Makes sure we can create an output stream for the default output device.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       MakeOutputStream_ForDefaultDevice) {
  MockOutputDeviceMixer* mock_mixer =
      SetUpMockMixerCreation(current_default_device_id_);

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*mock_mixer, MakeMixableStream(ExactParams(default_params_),
                                             ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      default_device_id(), default_params_, GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure we can create an output stream for a device ID that happens to be
// the current default.
TEST_F(OutputDeviceMixerManagerTest,
       MakeOutputStream_ForSpecificDeviceId_IdIsDefault) {
  ASSERT_EQ(kFakeDeviceId, current_default_device_id_);

  MockOutputDeviceMixer* mock_mixer = SetUpMockMixerCreation(kFakeDeviceId);

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*mock_mixer, MakeMixableStream(ExactParams(default_params_),
                                             ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure we can create an output stream a device ID for a device that is
// not the default device.
TEST_F(OutputDeviceMixerManagerTest,
       MakeOutputStream_ForSpecificDeviceId_IdIsNotDefault) {
  ASSERT_NE(kOtherFakeDeviceId, current_default_device_id_);

  MockOutputDeviceMixer* mock_mixer =
      SetUpMockMixerCreation(kOtherFakeDeviceId);

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*mock_mixer, MakeMixableStream(ExactParams(default_params_),
                                             ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kOtherFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure we still get an unmixable stream when requesting bitstream
// formats.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_WithBitstreamFormat) {
  ExpectNoMixerCreated();

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(audio_manager_, MakeAudioOutputStreamProxy(_, _))
      .WillOnce(Return(&mock_stream));

  AudioParameters bitstream_params{AudioParameters::Format::AUDIO_BITSTREAM_AC3,
                                   media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                                   /*sample_rate=*/8000,
                                   /*frames_per_buffer=*/800};

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, bitstream_params, GetNoopDeviceChangeCallback());

  EXPECT_TRUE(out_stream);

  // Test cleanup.
  out_stream->Close();
}

// Makes sure we handle running out of stream proxies.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_MaxProxies) {
  ExpectNoMixerCreated();

  EXPECT_CALL(audio_manager_, MakeAudioOutputStreamProxy(_, _))
      .WillOnce(Return(nullptr));

  // We use bitstream parameters to simplify hitting a portion of the code that
  // creates an AudioOutputStream directly.
  AudioParameters bitstream_params{AudioParameters::Format::AUDIO_BITSTREAM_AC3,
                                   media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                                   /*sample_rate=*/8000,
                                   /*frames_per_buffer=*/800};

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, bitstream_params, GetNoopDeviceChangeCallback());

  EXPECT_FALSE(out_stream);
}

// Makes sure we handle failing to create a mixer.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_MixerCreationFails) {
  EXPECT_CALL(*this,
              CreateOutputDeviceMixerCalled(
                  kFakeDeviceId, CompatibleParams(default_params_), _, _))
      .WillOnce(Return(ByMove(nullptr)));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_FALSE(out_stream);
}

// Makes sure we handle the case when the output mixer returns a nullptr when
// creating a stream.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_MixerReturnsNull) {
  MockOutputDeviceMixer* mock_mixer = SetUpMockMixerCreation(kFakeDeviceId);

  EXPECT_CALL(*mock_mixer, MakeMixableStream(ExactParams(default_params_),
                                             ValidDeviceChangeCallback()))
      .WillOnce(Return(nullptr));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_FALSE(out_stream);
}

// Makes sure creating multiple output streams for the same device ID re-uses
// the same OutputDeviceMixer.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_OneMixerPerId) {
  MockOutputDeviceMixer* mock_mixer = SetUpMockMixerCreation(kFakeDeviceId);

  MockAudioOutputStream mock_stream_a;
  MockAudioOutputStream mock_stream_b;
  EXPECT_CALL(*mock_mixer, MakeMixableStream(ExactParams(default_params_),
                                             ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream_b))
      .WillOnce(Return(&mock_stream_a));

  // This call should create an OutputDeviceMixer.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  // This call should re-use the OutputDeviceMixer.
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure creating an output stream for the "default ID" or the
// "current default device" is equivalent, and the mixers are shared.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       MakeOutputStream_DefaultIdAndCurrentDefaultShareOneMixer) {
  MockOutputDeviceMixer* mock_mixer =
      SetUpMockMixerCreation(current_default_device_id_);

  MockAudioOutputStream mock_stream_a;
  MockAudioOutputStream mock_stream_b;
  EXPECT_CALL(*mock_mixer, MakeMixableStream(ExactParams(default_params_),
                                             ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream_b))
      .WillOnce(Return(&mock_stream_a));

  // This call should create an OutputDeviceMixer.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      current_default_device_id_, default_params_,
      GetNoopDeviceChangeCallback());

  // This call should re-use the same OutputDeviceMixer.
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      default_device_id(), default_params_, GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure we create one output mixer per device ID.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_TwoDevicesTwoMixers) {
  InSequence s;
  MockOutputDeviceMixer* mock_mixer_a = SetUpMockMixerCreation(kFakeDeviceId);

  MockAudioOutputStream mock_stream_a;
  EXPECT_CALL(*mock_mixer_a, MakeMixableStream(ExactParams(default_params_),
                                               ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream_a));

  MockOutputDeviceMixer* mock_mixer_b =
      SetUpMockMixerCreation(kOtherFakeDeviceId);

  MockAudioOutputStream mock_stream_b;
  EXPECT_CALL(*mock_mixer_b, MakeMixableStream(ExactParams(default_params_),
                                               ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream_b));

  // Create the first OutputDeviceMixer.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  // Create a second OutputDeviceMixer.
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      kOtherFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure we get the latest default device ID each time we create a stream
// for the default device ID.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       MakeOutputStream_CurrentDefaultIsUpdatedAfterDeviceChange) {
  const std::string& first_default_id = current_default_device_id_;
  const std::string& second_default_id = kOtherFakeDeviceId;

  ASSERT_NE(first_default_id, second_default_id);

  MockOutputDeviceMixer* mock_mixer_a =
      SetUpMockMixerCreation(first_default_id);
  MockOutputDeviceMixer* mock_mixer_b =
      SetUpMockMixerCreation(second_default_id);

  MockAudioOutputStream mock_stream_a;
  EXPECT_CALL(*mock_mixer_a, MakeMixableStream(ExactParams(default_params_),
                                               ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream_a));

  MockAudioOutputStream mock_stream_b;
  EXPECT_CALL(*mock_mixer_b, MakeMixableStream(ExactParams(default_params_),
                                               ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream_b));

  // Create the first OutputDeviceMixer.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      default_device_id(), default_params_, GetNoopDeviceChangeCallback());

  // Force the manager to get the latest current default device ID.
  SimulateDeviceChange(second_default_id);

  // Create a second OutputDeviceMixer.
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      default_device_id(), default_params_, GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure OutputDeviceMixers are notified of device changes.
TEST_F(OutputDeviceMixerManagerTest,
       OnDeviceChange_MixersReceiveDeviceChanges) {
  // We don't care about the streams these devices will create.
  InSequence s;
  MockOutputDeviceMixer* pre_mock_mixer_a =
      SetUpMockMixer_NoStreams(kFakeDeviceId);
  MockOutputDeviceMixer* pre_mock_mixer_b =
      SetUpMockMixer_NoStreams(kOtherFakeDeviceId);

  EXPECT_CALL(*pre_mock_mixer_a, ProcessDeviceChange()).Times(1);
  EXPECT_CALL(*pre_mock_mixer_b, ProcessDeviceChange()).Times(1);

  MockOutputDeviceMixer* post_mock_mixer_a =
      SetUpMockMixer_NoStreams(kFakeDeviceId);
  EXPECT_CALL(*post_mock_mixer_a, ProcessDeviceChange()).Times(0);

  // Create the OutputDeviceMixers.
  output_mixer_manager_.MakeOutputStream(kFakeDeviceId, default_params_,
                                         GetNoopDeviceChangeCallback());

  output_mixer_manager_.MakeOutputStream(kOtherFakeDeviceId, default_params_,
                                         GetNoopDeviceChangeCallback());

  // Trigger the calls to ProcessDeviceChange()
  SimulateDeviceChange();

  // Force the recreation of output mixers, |post_mock_mixer_a| in this case.
  output_mixer_manager_.MakeOutputStream(kFakeDeviceId, default_params_,
                                         GetNoopDeviceChangeCallback());
}

// Attach/detach listeners with no mixer.
TEST_F(OutputDeviceMixerManagerTest, DeviceOutputListener_StartStop) {
  ExpectNoMixerCreated();

  StrictMock<MockListener> listener;

  // Attach/detach multiple listeners to/from multiple devices.
  output_mixer_manager_.StartListening(&listener, kFakeDeviceId);
  output_mixer_manager_.StopListening(&listener, kFakeDeviceId);
}

// Attach/detach listeners to multiple devices with no mixers.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStop_MultipleDevice) {
  ExpectNoMixerCreated();

  StrictMock<MockListener> listener_a;
  StrictMock<MockListener> listener_b;

  output_mixer_manager_.StartListening(&listener_a, kFakeDeviceId);
  output_mixer_manager_.StartListening(&listener_b, kOtherFakeDeviceId);

  output_mixer_manager_.StopListening(&listener_a, kFakeDeviceId);
  output_mixer_manager_.StopListening(&listener_b, kOtherFakeDeviceId);
}

// Attach/detach multiple listeners to a single device with no mixer.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStop_MultipleListener) {
  ExpectNoMixerCreated();

  StrictMock<MockListener> listener_a;
  StrictMock<MockListener> listener_b;

  output_mixer_manager_.StartListening(&listener_a, kFakeDeviceId);
  output_mixer_manager_.StartListening(&listener_b, kFakeDeviceId);

  output_mixer_manager_.StopListening(&listener_a, kFakeDeviceId);
  output_mixer_manager_.StopListening(&listener_b, kFakeDeviceId);
}

// Attach/detach to the default device.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_StartStop_DefaultId) {
  ExpectNoMixerCreated();

  StrictMock<MockListener> listener;

  output_mixer_manager_.StartListening(&listener, default_device_id());
  output_mixer_manager_.StopListening(&listener, default_device_id());
}

// Listeners are attached as they are added.
TEST_F(OutputDeviceMixerManagerTest, DeviceOutputListener_CreateStartStop) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kFakeDeviceId);

  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 1);

  ForceOutputMixerCreation(kFakeDeviceId);
  output_mixer_manager_.StartListening(listener.get(), kFakeDeviceId);
  output_mixer_manager_.StopListening(listener.get(), kFakeDeviceId);
}

// Listeners are attached on mixer creation.
TEST_F(OutputDeviceMixerManagerTest, DeviceOutputListener_StartCreateStop) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kFakeDeviceId);

  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 1);

  output_mixer_manager_.StartListening(listener.get(), kFakeDeviceId);
  ForceOutputMixerCreation(kFakeDeviceId);
  output_mixer_manager_.StopListening(listener.get(), kFakeDeviceId);
}

// Removed listeners are not attached.
TEST_F(OutputDeviceMixerManagerTest, DeviceOutputListener_StartStopCreate) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kFakeDeviceId);

  auto listener = GetListenerWithStartStopExpectations(mixer, 0, 0);

  output_mixer_manager_.StartListening(listener.get(), kFakeDeviceId);
  output_mixer_manager_.StopListening(listener.get(), kFakeDeviceId);
  ForceOutputMixerCreation(kFakeDeviceId);
}

// Removed listeners are not attached, and remaining listeners are.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStopCreate_TwoListeners) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kFakeDeviceId);

  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 0);
  auto removed_listener = GetListenerWithStartStopExpectations(mixer, 0, 0);

  output_mixer_manager_.StartListening(listener.get(), kFakeDeviceId);
  output_mixer_manager_.StartListening(removed_listener.get(), kFakeDeviceId);
  output_mixer_manager_.StopListening(removed_listener.get(), kFakeDeviceId);
  ForceOutputMixerCreation(kFakeDeviceId);
}

TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_CreateStartStop_DefaultId) {
  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_default_device_id_);

  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 1);

  ForceOutputMixerCreation(default_device_id());
  output_mixer_manager_.StartListening(listener.get(), default_device_id());
  output_mixer_manager_.StopListening(listener.get(), default_device_id());
}

TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_StartCreateStop_DefaultId) {
  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_default_device_id_);

  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 1);

  output_mixer_manager_.StartListening(listener.get(), default_device_id());
  ForceOutputMixerCreation(default_device_id());
  output_mixer_manager_.StopListening(listener.get(), default_device_id());
}

// Makes sure default-listeners are attached to the current-default-device-mixer
// when it is created.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_DefaultIdListenersAttachToCurrentDefaultMixer) {
  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_default_device_id_);

  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 1);

  output_mixer_manager_.StartListening(listener.get(), default_device_id());
  ForceOutputMixerCreation(current_default_device_id_);
  output_mixer_manager_.StopListening(listener.get(), default_device_id());
}

// Makes sure current-default-device-listeners are attached when the
// default-device-mixer is created.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_CurrentDefaultListenersAttachToDefaultIdMixer) {
  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_default_device_id_);

  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 1);

  output_mixer_manager_.StartListening(listener.get(),
                                       current_default_device_id_);
  ForceOutputMixerCreation(default_device_id());
  output_mixer_manager_.StopListening(listener.get(),
                                      current_default_device_id_);
}

// Makes sure the presence of listeners does not force device recreation
// on device change.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_NoCreateAfterDeviceChange_WithListeners) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kFakeDeviceId);

  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 0);

  ForceOutputMixerCreation(kFakeDeviceId);
  output_mixer_manager_.StartListening(listener.get(), kFakeDeviceId);

  SimulateDeviceChange();

  output_mixer_manager_.StopListening(listener.get(), kFakeDeviceId);
}

// Makes sure listeners are re-attached when mixers are recreated.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_ListenersReattachedAfterDeviceChange) {
  // Setup pre-device-change expectations
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kFakeDeviceId);
  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 0);

  ForceOutputMixerCreation(kFakeDeviceId);
  output_mixer_manager_.StartListening(listener.get(), kFakeDeviceId);

  SimulateDeviceChange();

  // Clear expectations so we can set up new ones.
  testing::Mock::VerifyAndClearExpectations(this);
  testing::Mock::VerifyAndClearExpectations(listener.get());

  // Setup post-device-change expectations
  MockOutputDeviceMixer* new_mixer = SetUpMockMixer_NoStreams(kFakeDeviceId);
  EXPECT_CALL(*new_mixer, StartListening(listener.get())).Times(1);

  ForceOutputMixerCreation(kFakeDeviceId);
}

// Makes sure the default listeners are re-attached when mixers are re-created.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_DefaultIdListenersReattachedAfterDeviceChange) {
  // Setup pre-device-change expectations
  const std::string& kInitialDefaultId = current_default_device_id_;
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kInitialDefaultId);
  auto listener = GetListenerWithStartStopExpectations(mixer, 1, 0);

  // Setup post-device-change expectations
  const std::string& kNewDefaultId = kOtherFakeDeviceId;
  MockOutputDeviceMixer* new_mixer = SetUpMockMixer_NoStreams(kNewDefaultId);
  EXPECT_CALL(*new_mixer, StartListening(listener.get())).Times(1);

  output_mixer_manager_.StartListening(listener.get(), default_device_id());

  // Listener should be attached to |mixer|.
  ForceOutputMixerCreation(kInitialDefaultId);

  SimulateDeviceChange(kNewDefaultId);

  // Listener should be attached to |new_mixer|.
  ForceOutputMixerCreation(kNewDefaultId);
}

// Makes sure both the "default listeners" and "current default device"
// listeners get attached to the "current default device" mixer.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_CurrentDefaultMixerCreation_ListenersAttached) {
  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_default_device_id_);
  auto default_listener = GetListenerWithStartStopExpectations(mixer, 1, 0);
  auto current_default_device_listener =
      GetListenerWithStartStopExpectations(mixer, 1, 0);
  auto other_listener = GetListenerWithStartStopExpectations(mixer, 0, 0);

  output_mixer_manager_.StartListening(default_listener.get(),
                                       default_device_id());
  output_mixer_manager_.StartListening(current_default_device_listener.get(),
                                       current_default_device_id_);

  ASSERT_NE(kOtherFakeDeviceId, current_default_device_id_);
  output_mixer_manager_.StartListening(other_listener.get(),
                                       kOtherFakeDeviceId);

  // |other_listener| should not be attached to |mixer|.
  ForceOutputMixerCreation(current_default_device_id_);
}

// Makes sure both the "default listeners" and "current default device"
// listeners get attached to the "default ID" mixer.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_DefaultIdMixerCreation_ListenersAttached) {
  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_default_device_id_);
  auto default_listener = GetListenerWithStartStopExpectations(mixer, 1, 0);
  auto current_default_device_listener =
      GetListenerWithStartStopExpectations(mixer, 1, 0);
  auto other_listener = GetListenerWithStartStopExpectations(mixer, 0, 0);

  output_mixer_manager_.StartListening(default_listener.get(),
                                       default_device_id());
  output_mixer_manager_.StartListening(current_default_device_listener.get(),
                                       current_default_device_id_);

  ASSERT_NE(kOtherFakeDeviceId, current_default_device_id_);
  output_mixer_manager_.StartListening(other_listener.get(),
                                       kOtherFakeDeviceId);

  // |other_listener| should not be attached to |mixer|.
  ForceOutputMixerCreation(default_device_id());
}

// Makes sure listeners attached to devices that aren't the "current default
// device" aren't attached to the "current default device" mixer.
TEST_P(OutputDeviceMixerManagerTestWithDefault,
       DeviceOutputListener_OtherDeviceMixerCreation_ListenersNotAttached) {
  MockOutputDeviceMixer* other_mixer =
      SetUpMockMixer_NoStreams(kOtherFakeDeviceId);
  auto default_listener =
      GetListenerWithStartStopExpectations(other_mixer, 0, 0);
  auto current_default_device_listener =
      GetListenerWithStartStopExpectations(other_mixer, 0, 0);
  auto other_listener = GetListenerWithStartStopExpectations(other_mixer, 1, 0);

  output_mixer_manager_.StartListening(default_listener.get(),
                                       default_device_id());
  output_mixer_manager_.StartListening(current_default_device_listener.get(),
                                       current_default_device_id_);

  ASSERT_NE(kOtherFakeDeviceId, current_default_device_id_);
  output_mixer_manager_.StartListening(other_listener.get(),
                                       kOtherFakeDeviceId);

  // Only |other_listener| should be attached to |other_mixer|.
  ForceOutputMixerCreation(kOtherFakeDeviceId);
}

INSTANTIATE_TEST_SUITE_P(DefaultDeviceIdTests,
                         OutputDeviceMixerManagerTestWithDefault,
                         testing::Values(kDefaultDeviceId, kEmptyDeviceId));

}  // namespace audio
