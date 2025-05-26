// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_device_mixer_manager.h"

#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "services/audio/output_device_mixer.h"
#include "services/audio/reference_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// The "default" and "communications" strings represent reserved device IDs.
// They are used in different situations, but the OutputDeviceMixerManager
// should treat all reserved device IDs the same way.
enum class ReservedIdTestType {
  kDefault,
  kCommunications,
};

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
const std::string kFakeCommunicationsId = "0xabcd";
const std::string kEmptyDeviceId = std::string();
const std::string kNormalizedDefaultDeviceId = kEmptyDeviceId;
const auto* kReservedDefaultId =
    media::AudioDeviceDescription::kDefaultDeviceId;
const auto* kReservedCommsId =
    media::AudioDeviceDescription::kCommunicationsDeviceId;

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
  MOCK_METHOD(std::string, GetCommunicationsOutputDeviceID, (), (override));
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
    : public ::testing::TestWithParam<ReservedIdTestType> {
 public:
  OutputDeviceMixerManagerTest()
      : current_default_physical_device_id_(kFakeDeviceId),
        current_communications_physical_device_id_(kFakeCommunicationsId),
        default_params_(AudioParameters::Format::AUDIO_PCM_LOW_LATENCY,
                        media::ChannelLayoutConfig::Stereo(),
                        /*sample_rate=*/8000,
                        /*frames_per_buffer=*/800),
        output_mixer_manager_(
            &audio_manager_,
            base::BindRepeating(
                &OutputDeviceMixerManagerTest::CreateOutputDeviceMixerCalled,
                base::Unretained(this))) {
    EXPECT_CALL(audio_manager_, GetOutputStreamParameters(_))
        .WillRepeatedly(Return(default_params_));

    EXPECT_CALL(audio_manager_,
                GetOutputStreamParameters(
                    media::AudioDeviceDescription::kDefaultDeviceId))
        .WillRepeatedly(Return(default_params_));

    EXPECT_CALL(audio_manager_, GetDefaultOutputDeviceID()).WillRepeatedly([&] {
      return audio_manager_supports_default_physical_id_
                 ? current_default_physical_device_id_
                 : kEmptyDeviceId;
    });

    EXPECT_CALL(audio_manager_, GetCommunicationsOutputDeviceID())
        .WillRepeatedly([&] {
          return audio_manager_supports_communications_physical_id_
                     ? current_communications_physical_device_id_
                     : kEmptyDeviceId;
        });

    // Force |output_mixer_manager_| to pick up the latest default device ID
    // from AudioManager::GetDefaultOutputDeviceID().
    output_mixer_manager_.OnDeviceChange();
  }

  ~OutputDeviceMixerManagerTest() override { audio_manager_.Shutdown(); }

  MOCK_METHOD(std::unique_ptr<OutputDeviceMixer>,
              CreateOutputDeviceMixerCalled,
              (const std::string&,
               const media::AudioParameters&,
               OutputDeviceMixer::CreateStreamCallback,
               scoped_refptr<base::SingleThreadTaskRunner>));

 protected:
  std::string current_default_physical_device() {
    return current_default_physical_device_id_;
  }

  std::string current_communications_physical_device() {
    return current_communications_physical_device_id_;
  }

  void SetAudioManagerDefaultIdSupport(bool support) {
    bool needs_device_change =
        audio_manager_supports_default_physical_id_ != support;
    audio_manager_supports_default_physical_id_ = support;

    // Force |output_mixer_manager_| to pick up the latest default device ID.
    if (needs_device_change)
      output_mixer_manager_.OnDeviceChange();
  }

  void SetAudioManagerCommunicationsIdSupport(bool support) {
    bool needs_device_change =
        audio_manager_supports_communications_physical_id_ != support;
    audio_manager_supports_communications_physical_id_ = support;

    // Force |output_mixer_manager_| to pick up the latest default device ID.
    if (needs_device_change)
      output_mixer_manager_.OnDeviceChange();
  }

  MockOutputDeviceMixer* SetUpMockMixerCreation(
      std::string device_id = kNormalizedDefaultDeviceId) {
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
  MockOutputDeviceMixer* SetUpMockMixer_NoStreams(
      std::string device_id = kNormalizedDefaultDeviceId) {
    MockOutputDeviceMixer* output_mixer = SetUpMockMixerCreation(device_id);

    EXPECT_CALL(*output_mixer, MakeMixableStream(_, _))
        .WillRepeatedly(Return(nullptr));

    return output_mixer;
  }

  std::unique_ptr<NiceMock<MockListener>> GetListener_MixerExpectsStartStop(
      MockOutputDeviceMixer* mixer) {
    return GetListenerWithStartStopExpectations(mixer, 1, 1);
  }

  std::unique_ptr<NiceMock<MockListener>> GetListener_MixerExpectsStart(
      MockOutputDeviceMixer* mixer) {
    return GetListenerWithStartStopExpectations(mixer, 1, 0);
  }

  std::unique_ptr<NiceMock<MockListener>> GetListener_MixerExpectsNoCalls(
      MockOutputDeviceMixer* mixer) {
    return GetListenerWithStartStopExpectations(mixer, 0, 0);
  }

  void ForceOutputMixerCreation(const std::string& device_id) {
    output_mixer_manager_.MakeOutputStream(device_id, default_params_,
                                           GetNoopDeviceChangeCallback());
  }

  void SimulateDeviceChange() {
    SimulateDeviceChange(std::nullopt, std::nullopt);
  }

  void SimulateDeviceChange(
      std::optional<std::string> new_default_physical_device,
      std::optional<std::string> new_communications_physical_device) {
    if (new_default_physical_device)
      current_default_physical_device_id_ = *new_default_physical_device;

    if (new_communications_physical_device) {
      current_communications_physical_device_id_ =
          *new_communications_physical_device;
    }

    output_mixer_manager_.OnDeviceChange();
  }

  void ExpectNoMixerCreated() {
    EXPECT_CALL(*this, CreateOutputDeviceMixerCalled(_, _, _, _)).Times(0);
  }

  base::OnceClosure GetOnDeviceChangeCallback() {
    return output_mixer_manager_.GetOnDeviceChangeCallback();
  }

  // Syntactic sugar, to differentiate from base::OnceClosure in tests.
  base::OnceClosure GetNoopDeviceChangeCallback() { return base::DoNothing(); }

  // ----------------------------------------------------------
  // The following methods are use to parameterize tests that are identical for
  // the "communications" and "default" reserved IDs.
  //

  // Whether we are testing the "default" or "communications" reserved ID.
  ReservedIdTestType reserved_id_test_type() { return GetParam(); }

  void SetAudioManagerReservedIdSupport(bool support) {
    switch (reserved_id_test_type()) {
      case ReservedIdTestType::kDefault:
        return SetAudioManagerDefaultIdSupport(support);
      case ReservedIdTestType::kCommunications:
        return SetAudioManagerCommunicationsIdSupport(support);
    }
  }

  MockOutputDeviceMixer* SetUpReservedMixerCreation() {
    switch (reserved_id_test_type()) {
      case ReservedIdTestType::kDefault:
        return SetUpMockMixerCreation(kNormalizedDefaultDeviceId);
      case ReservedIdTestType::kCommunications:
        return SetUpMockMixerCreation(kReservedCommsId);
    }
  }

  MockOutputDeviceMixer* SetUpReservedMixer_NoStreams() {
    switch (reserved_id_test_type()) {
      case ReservedIdTestType::kDefault:
        return SetUpMockMixer_NoStreams(kNormalizedDefaultDeviceId);
      case ReservedIdTestType::kCommunications:
        return SetUpMockMixer_NoStreams(kReservedCommsId);
    }
  }

  std::string reserved_device_id() {
    switch (reserved_id_test_type()) {
      case ReservedIdTestType::kDefault:
        return kReservedDefaultId;
      case ReservedIdTestType::kCommunications:
        return kReservedCommsId;
    }
  }

  std::string current_reserved_physical_device() {
    switch (reserved_id_test_type()) {
      case ReservedIdTestType::kDefault:
        return current_default_physical_device();
      case ReservedIdTestType::kCommunications:
        return current_communications_physical_device();
    }
  }

  void SimulateReservedDeviceChange(std::string new_reserved_physical_id) {
    switch (reserved_id_test_type()) {
      case ReservedIdTestType::kDefault:
        SimulateDeviceChange(new_reserved_physical_id, std::nullopt);
        return;
      case ReservedIdTestType::kCommunications:
        SimulateDeviceChange(std::nullopt, new_reserved_physical_id);
        return;
    }
  }

  bool audio_manager_supports_default_physical_id_ = true;
  bool audio_manager_supports_communications_physical_id_ = true;

  // Simulate the value that would be returned by
  // AudioManager::GetDefaultOutputDeviceId() if it is supported.
  std::string current_default_physical_device_id_;

  // Simulate the value that would be returned by
  // AudioManager::GetCommunicationsOutputDeviceId() if it is supported.
  std::string current_communications_physical_device_id_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  AudioParameters default_params_;
  NiceMock<LocalMockAudioManager> audio_manager_;
  OutputDeviceMixerManager output_mixer_manager_;

 private:
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
};

// Makes sure we can create an output stream for the reserved output devices.
TEST_P(OutputDeviceMixerManagerTest, MakeOutputStream_ForReservedDevice) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixerCreation();

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*reserved_mixer, MakeMixableStream(ExactParams(default_params_),
                                                 ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      reserved_device_id(), default_params_, GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure we can create a default output stream when AudioManager doesn't
// support getting the current default ID.
TEST_P(OutputDeviceMixerManagerTest,
       MakeOutputStream_ForReservedDevice_NoGetReservedOuputDeviceIdSupport) {
  SetAudioManagerReservedIdSupport(false);

  // Note: kReservedCommsId maps to kNormalizedDefaultDeviceId when
  //       |!audio_manager_supports_communications_physical_id_|.
  MockOutputDeviceMixer* reserved_mixer =
      SetUpMockMixerCreation(kNormalizedDefaultDeviceId);

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*reserved_mixer, MakeMixableStream(ExactParams(default_params_),
                                                 ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      reserved_device_id(), default_params_, GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure the empty string resolves to the "default" device.
TEST_F(OutputDeviceMixerManagerTest,
       MakeOutputStream_ForDefaultDevice_EmptyDeviceId) {
  MockOutputDeviceMixer* default_mixer = SetUpMockMixerCreation();

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*default_mixer, MakeMixableStream(ExactParams(default_params_),
                                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  // kEmptyDeviceId should be treated the same as kReservedDefaultId.
  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kEmptyDeviceId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure we can create an output stream for physical IDs that match a
// reserved ID's.
TEST_P(OutputDeviceMixerManagerTest,
       MakeOutputStream_ForSpecificDeviceId_MatchesCurrentReservedId) {
  SetAudioManagerReservedIdSupport(true);

  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixerCreation();

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*reserved_mixer, MakeMixableStream(ExactParams(default_params_),
                                                 ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  // Getting a stream for current_reserved_physical_device() should create
  // the |reserved_mixer| instead of a mixer for that physical ID.
  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      current_reserved_physical_device(), default_params_,
      GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure we can create an output stream for a device ID when
// AudioManager::GetDefaultOutputDeviceId() is unsupported.
TEST_P(OutputDeviceMixerManagerTest,
       MakeOutputStream_ForSpecificDeviceId_NoGetDefaultOuputDeviceIdSupport) {
  SetAudioManagerDefaultIdSupport(false);

  // A mixer for the physical device ID should be created, instead of the
  // default mixer.
  MockOutputDeviceMixer* physical_device_mixer =
      SetUpMockMixerCreation(current_default_physical_device());

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*physical_device_mixer,
              MakeMixableStream(ExactParams(default_params_),
                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      current_default_physical_device(), default_params_,
      GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure we can create an output stream for a device ID when
// AudioManager doesn't support getting the current reserved ID.
TEST_P(OutputDeviceMixerManagerTest,
       MakeOutputStream_ForSpecificDeviceId_NoGetGetReservedIdSupport) {
  SetAudioManagerReservedIdSupport(false);

  // A mixer for the physical device ID should be created, instead of the
  // reserved mixer.
  MockOutputDeviceMixer* physical_device_mixer =
      SetUpMockMixerCreation(current_reserved_physical_device());

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(*physical_device_mixer,
              MakeMixableStream(ExactParams(default_params_),
                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      current_reserved_physical_device(), default_params_,
      GetNoopDeviceChangeCallback());

  EXPECT_EQ(&mock_stream, out_stream);
}

// Makes sure we can create an output stream a device ID for a device that is
// not any device.
TEST_F(OutputDeviceMixerManagerTest,
       MakeOutputStream_ForSpecificDeviceId_IdDoesntMatchReservedIds) {
  ASSERT_NE(kOtherFakeDeviceId, current_default_physical_device());
  ASSERT_NE(kOtherFakeDeviceId, current_communications_physical_device());

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

// Makes sure we get the correct output parameters from the AudioManager when
// creating streams.
TEST_F(OutputDeviceMixerManagerTest,
       MakeOutputStream_GetsDeviceOrDefaultParams) {
  // Reset default test setup expectations.
  testing::Mock::VerifyAndClearExpectations(&audio_manager_);

  SetUpMockMixerCreation();

  EXPECT_CALL(audio_manager_,
              GetOutputStreamParameters(kNormalizedDefaultDeviceId))
      .WillOnce(Return(default_params_));

  output_mixer_manager_.MakeOutputStream(kReservedDefaultId, default_params_,
                                         GetNoopDeviceChangeCallback());

  testing::Mock::VerifyAndClearExpectations(this);
  testing::Mock::VerifyAndClearExpectations(&audio_manager_);

  SetUpMockMixerCreation(kOtherFakeDeviceId);

  EXPECT_CALL(audio_manager_, GetOutputStreamParameters(kOtherFakeDeviceId))
      .WillOnce(Return(default_params_));

  output_mixer_manager_.MakeOutputStream(kOtherFakeDeviceId, default_params_,
                                         GetNoopDeviceChangeCallback());
}

// Makes sure we still get an unmixable stream when requesting bitstream
// formats.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_WithBitstreamFormat) {
  ExpectNoMixerCreated();

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(audio_manager_, MakeAudioOutputStreamProxy(_, _))
      .WillOnce(Return(&mock_stream));

  AudioParameters bitstream_params{AudioParameters::Format::AUDIO_BITSTREAM_AC3,
                                   media::ChannelLayoutConfig::Stereo(),
                                   /*sample_rate=*/8000,
                                   /*frames_per_buffer=*/800};

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kOtherFakeDeviceId, bitstream_params, GetNoopDeviceChangeCallback());

  EXPECT_TRUE(out_stream);

  // Test cleanup.
  out_stream->Close();
}

// Makes sure we still get an unmixable stream if device info is stale and
// AudioManager::GetOutputStreamParameters() returns invalid parameters.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_WithStaleDeviceInfo) {
  EXPECT_CALL(audio_manager_,
              GetOutputStreamParameters(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(0);

  // Return invalid parameters, which should fail mixer creation.
  EXPECT_CALL(audio_manager_, GetOutputStreamParameters(kOtherFakeDeviceId))
      .WillOnce(Return(media::AudioParameters()));

  ExpectNoMixerCreated();

  MockAudioOutputStream mock_stream;
  EXPECT_CALL(audio_manager_, MakeAudioOutputStreamProxy(_, _))
      .WillOnce(Return(&mock_stream));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kOtherFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

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
                                   media::ChannelLayoutConfig::Stereo(),
                                   /*sample_rate=*/8000,
                                   /*frames_per_buffer=*/800};

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kOtherFakeDeviceId, bitstream_params, GetNoopDeviceChangeCallback());

  EXPECT_FALSE(out_stream);
}

// Makes sure we handle failing to create a mixer.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_MixerCreationFails) {
  EXPECT_CALL(*this, CreateOutputDeviceMixerCalled(
                         kNormalizedDefaultDeviceId,
                         CompatibleParams(default_params_), _, _))
      .WillOnce(Return(ByMove(nullptr)));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kReservedDefaultId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_FALSE(out_stream);
}

// Makes sure we handle the case when the output mixer returns a nullptr when
// creating a stream.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_MixerReturnsNull) {
  MockOutputDeviceMixer* default_mixer = SetUpMockMixerCreation();

  EXPECT_CALL(*default_mixer, MakeMixableStream(ExactParams(default_params_),
                                                ValidDeviceChangeCallback()))
      .WillOnce(Return(nullptr));

  AudioOutputStream* out_stream = output_mixer_manager_.MakeOutputStream(
      kReservedDefaultId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_FALSE(out_stream);
}

// Makes sure creating multiple output streams for the same device ID re-uses
// the same OutputDeviceMixer.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_OneMixerPerId) {
  MockOutputDeviceMixer* physical_id_mixer =
      SetUpMockMixerCreation(kOtherFakeDeviceId);

  MockAudioOutputStream mock_stream_a;
  MockAudioOutputStream mock_stream_b;
  EXPECT_CALL(*physical_id_mixer,
              MakeMixableStream(ExactParams(default_params_),
                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream_b))
      .WillOnce(Return(&mock_stream_a));

  // This call should create an OutputDeviceMixer.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      kOtherFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  // This call should re-use the OutputDeviceMixer.
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      kOtherFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure creating an output stream for a "reserved ID" or the
// "current reserved device ID" is equivalent, and the mixer is shared.
TEST_P(OutputDeviceMixerManagerTest,
       MakeOutputStream_ReservedIdAndCurrentReservedDeviceIdShareOneMixer) {
  MockOutputDeviceMixer* special_mixer = SetUpReservedMixerCreation();

  MockAudioOutputStream mock_stream_a;
  MockAudioOutputStream mock_stream_b;
  EXPECT_CALL(*special_mixer, MakeMixableStream(ExactParams(default_params_),
                                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&mock_stream_b))
      .WillOnce(Return(&mock_stream_a));

  // This call should create an OutputDeviceMixer.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      current_reserved_physical_device(), default_params_,
      GetNoopDeviceChangeCallback());

  // This call should re-use the same OutputDeviceMixer.
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      reserved_device_id(), default_params_, GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure we create one output mixer per device ID.
TEST_F(OutputDeviceMixerManagerTest, MakeOutputStream_TwoDevicesTwoMixers) {
  SetAudioManagerDefaultIdSupport(false);
  SetAudioManagerCommunicationsIdSupport(false);

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

// Makes sure the default mixer is separate from other mixers.
TEST_F(OutputDeviceMixerManagerTest,
       MakeOutputStream_DefaultMixerDistinctFromOtherMixers) {
  SetAudioManagerDefaultIdSupport(false);

  InSequence s;
  MockOutputDeviceMixer* fake_device_mixer =
      SetUpMockMixerCreation(kFakeDeviceId);

  MockAudioOutputStream fake_stream;
  EXPECT_CALL(*fake_device_mixer,
              MakeMixableStream(ExactParams(default_params_),
                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&fake_stream));

  MockOutputDeviceMixer* default_mixer =
      SetUpMockMixerCreation(kNormalizedDefaultDeviceId);

  MockAudioOutputStream default_stream;
  EXPECT_CALL(*default_mixer, MakeMixableStream(ExactParams(default_params_),
                                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&default_stream));

  // Create the first OutputDeviceMixer.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      kFakeDeviceId, default_params_, GetNoopDeviceChangeCallback());

  // Create a second OutputDeviceMixer.
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      kReservedDefaultId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure the communications mixer is separate from other mixers.
TEST_F(OutputDeviceMixerManagerTest,
       MakeOutputStream_CommunicationsMixerDistinctFromOtherMixers) {
  SetAudioManagerCommunicationsIdSupport(false);

  InSequence s;
  MockOutputDeviceMixer* fake_device_mixer =
      SetUpMockMixerCreation(kFakeCommunicationsId);

  MockAudioOutputStream fake_stream;
  EXPECT_CALL(*fake_device_mixer,
              MakeMixableStream(ExactParams(default_params_),
                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&fake_stream));

  // Note: kReservedCommsId maps to kNormalizedDefaultDeviceId when
  //       |!audio_manager_supports_communications_physical_id_|.
  MockOutputDeviceMixer* default_mixer =
      SetUpMockMixerCreation(kNormalizedDefaultDeviceId);

  MockAudioOutputStream default_stream;
  EXPECT_CALL(*default_mixer, MakeMixableStream(ExactParams(default_params_),
                                                ValidDeviceChangeCallback()))
      .WillOnce(Return(&default_stream));

  // Create the first OutputDeviceMixer.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      kFakeCommunicationsId, default_params_, GetNoopDeviceChangeCallback());

  // Create a second OutputDeviceMixer.
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      kReservedCommsId, default_params_, GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure we get the latest reserved device ID each time we create a stream
// for a reserved ID.
TEST_P(OutputDeviceMixerManagerTest,
       MakeOutputStream_CurrentReseredIdIsUpdatedAfterDeviceChange) {
  SetAudioManagerReservedIdSupport(true);

  MockOutputDeviceMixer* reserved_mixer_a = SetUpReservedMixerCreation();

  MockAudioOutputStream reserved_stream_a;
  EXPECT_CALL(*reserved_mixer_a, MakeMixableStream(ExactParams(default_params_),
                                                   ValidDeviceChangeCallback()))
      .WillOnce(Return(&reserved_stream_a));

  // Force the creation of |reserved_mixer_a|.
  AudioOutputStream* out_stream_a = output_mixer_manager_.MakeOutputStream(
      current_reserved_physical_device(), default_params_,
      GetNoopDeviceChangeCallback());

  // Update the current default physical device.
  ASSERT_NE(current_reserved_physical_device(), kOtherFakeDeviceId);
  SimulateReservedDeviceChange(kOtherFakeDeviceId);
  ASSERT_EQ(current_reserved_physical_device(), kOtherFakeDeviceId);

  testing::Mock::VerifyAndClearExpectations(this);

  MockOutputDeviceMixer* reserved_mixer_b = SetUpReservedMixerCreation();

  MockAudioOutputStream reseved_stream_b;
  EXPECT_CALL(*reserved_mixer_b, MakeMixableStream(ExactParams(default_params_),
                                                   ValidDeviceChangeCallback()))
      .WillOnce(Return(&reseved_stream_b));

  // Force the creation of |reserved_mixer_b|, with a new
  // current_reserved_physical_device().
  AudioOutputStream* out_stream_b = output_mixer_manager_.MakeOutputStream(
      current_reserved_physical_device(), default_params_,
      GetNoopDeviceChangeCallback());

  EXPECT_NE(out_stream_a, out_stream_b);
}

// Makes sure OutputDeviceMixers are notified of device changes.
TEST_P(OutputDeviceMixerManagerTest,
       OnDeviceChange_MixersReceiveDeviceChanges) {
  SetAudioManagerReservedIdSupport(false);

  // We don't care about the streams these devices will create.
  InSequence s;
  MockOutputDeviceMixer* pre_mock_mixer_a =
      SetUpMockMixer_NoStreams(current_reserved_physical_device());
  MockOutputDeviceMixer* pre_mock_mixer_b =
      SetUpMockMixer_NoStreams(kOtherFakeDeviceId);

  // Note: kReservedCommsId maps to kNormalizedDefaultDeviceId when
  //       |!audio_manager_supports_communications_physical_id_|.
  MockOutputDeviceMixer* pre_mock_mixer_c =
      SetUpMockMixer_NoStreams(kNormalizedDefaultDeviceId);

  EXPECT_CALL(*pre_mock_mixer_a, ProcessDeviceChange()).Times(1);
  EXPECT_CALL(*pre_mock_mixer_b, ProcessDeviceChange()).Times(1);
  EXPECT_CALL(*pre_mock_mixer_c, ProcessDeviceChange()).Times(1);

  // Create the OutputDeviceMixers.
  output_mixer_manager_.MakeOutputStream(current_reserved_physical_device(),
                                         default_params_,
                                         GetNoopDeviceChangeCallback());

  output_mixer_manager_.MakeOutputStream(kOtherFakeDeviceId, default_params_,
                                         GetNoopDeviceChangeCallback());

  output_mixer_manager_.MakeOutputStream(reserved_device_id(), default_params_,
                                         GetNoopDeviceChangeCallback());

  // Trigger the calls to ProcessDeviceChange()
  SimulateDeviceChange();
}

// Makes sure OnDeviceChange() is only called once per device change.
TEST_F(OutputDeviceMixerManagerTest, OnDeviceChange_OncePerDeviceChange) {
  // Setup a mixer that expects exactly 1 device change.
  MockOutputDeviceMixer* default_mixer = SetUpMockMixer_NoStreams();
  EXPECT_CALL(*default_mixer, ProcessDeviceChange()).Times(1);

  // Create the mixer.
  ForceOutputMixerCreation(kReservedDefaultId);
  auto first_device_change_callback = GetOnDeviceChangeCallback();
  auto second_device_change_callback = GetOnDeviceChangeCallback();

  // |default_mixer| be notified of the device change.
  std::move(first_device_change_callback).Run();
  testing::Mock::VerifyAndClearExpectations(default_mixer);

  // Setup a new mixer.
  testing::Mock::VerifyAndClearExpectations(this);
  MockOutputDeviceMixer* new_default_mixer = SetUpMockMixer_NoStreams();

  // Make sure old callbacks don't trigger new device change events.
  EXPECT_CALL(*new_default_mixer, ProcessDeviceChange()).Times(0);
  ForceOutputMixerCreation(kReservedDefaultId);
  std::move(second_device_change_callback).Run();

  testing::Mock::VerifyAndClearExpectations(new_default_mixer);

  // Make sure the new mixer gets notified of changes through this new
  // callback.
  EXPECT_CALL(*new_default_mixer, ProcessDeviceChange()).Times(1);
  GetOnDeviceChangeCallback().Run();
}

// Attach/detach listeners with no mixer.
TEST_F(OutputDeviceMixerManagerTest, DeviceOutputListener_StartStop) {
  ExpectNoMixerCreated();

  StrictMock<MockListener> listener;

  // Attach/detach multiple listeners to/from multiple devices.
  output_mixer_manager_.StartListening(&listener, kFakeDeviceId);
  output_mixer_manager_.StopListening(&listener);
}

// Attach/detach listeners to multiple devices with no mixers.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStop_MultipleDevice) {
  ExpectNoMixerCreated();

  StrictMock<MockListener> listener_a;
  StrictMock<MockListener> listener_b;

  output_mixer_manager_.StartListening(&listener_a, kFakeDeviceId);
  output_mixer_manager_.StartListening(&listener_b, kOtherFakeDeviceId);

  output_mixer_manager_.StopListening(&listener_a);
  output_mixer_manager_.StopListening(&listener_b);
}

// Attach/detach multiple listeners to a single device with no mixer.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStop_MultipleListener) {
  ExpectNoMixerCreated();

  StrictMock<MockListener> listener_a;
  StrictMock<MockListener> listener_b;

  output_mixer_manager_.StartListening(&listener_a, kFakeDeviceId);
  output_mixer_manager_.StartListening(&listener_b, kFakeDeviceId);

  output_mixer_manager_.StopListening(&listener_a);
  output_mixer_manager_.StopListening(&listener_b);
}

// Attach/detach to the reserved device.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStop_ReservedId) {
  ExpectNoMixerCreated();

  StrictMock<MockListener> listener;

  output_mixer_manager_.StartListening(&listener, reserved_device_id());
  output_mixer_manager_.StopListening(&listener);
}

// Listeners are attached as they are added.
TEST_F(OutputDeviceMixerManagerTest, DeviceOutputListener_CreateStartStop) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kOtherFakeDeviceId);

  auto listener = GetListener_MixerExpectsStartStop(mixer);

  ForceOutputMixerCreation(kOtherFakeDeviceId);
  output_mixer_manager_.StartListening(listener.get(), kOtherFakeDeviceId);
  output_mixer_manager_.StopListening(listener.get());
}

// Listeners are attached on mixer creation.
TEST_F(OutputDeviceMixerManagerTest, DeviceOutputListener_StartCreateStop) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kOtherFakeDeviceId);

  auto listener = GetListener_MixerExpectsStartStop(mixer);

  output_mixer_manager_.StartListening(listener.get(), kOtherFakeDeviceId);
  ForceOutputMixerCreation(kOtherFakeDeviceId);
  output_mixer_manager_.StopListening(listener.get());
}

// Removed listeners are not attached.
TEST_F(OutputDeviceMixerManagerTest, DeviceOutputListener_StartStopCreate) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kOtherFakeDeviceId);

  auto listener = GetListener_MixerExpectsNoCalls(mixer);

  output_mixer_manager_.StartListening(listener.get(), kOtherFakeDeviceId);
  output_mixer_manager_.StopListening(listener.get());
  ForceOutputMixerCreation(kOtherFakeDeviceId);
}

// Listeners are attached as they are added.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_CreateStartStop_NoGetReservedIdSupport) {
  SetAudioManagerReservedIdSupport(false);

  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_reserved_physical_device());

  auto listener = GetListener_MixerExpectsStartStop(mixer);

  ForceOutputMixerCreation(current_reserved_physical_device());
  output_mixer_manager_.StartListening(listener.get(),
                                       current_reserved_physical_device());
  output_mixer_manager_.StopListening(listener.get());
}

// Listeners are attached on mixer creation.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartCreateStop_NoGetReservedIdSupport) {
  SetAudioManagerReservedIdSupport(false);

  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_reserved_physical_device());

  auto listener = GetListener_MixerExpectsStartStop(mixer);

  output_mixer_manager_.StartListening(listener.get(),
                                       current_reserved_physical_device());
  ForceOutputMixerCreation(current_reserved_physical_device());
  output_mixer_manager_.StopListening(listener.get());
}

// Removed listeners are not attached.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStopCreate_NoGetReservedIdSupport) {
  SetAudioManagerReservedIdSupport(false);

  MockOutputDeviceMixer* mixer =
      SetUpMockMixer_NoStreams(current_reserved_physical_device());

  auto listener = GetListener_MixerExpectsNoCalls(mixer);

  output_mixer_manager_.StartListening(listener.get(),
                                       current_reserved_physical_device());
  output_mixer_manager_.StopListening(listener.get());
  ForceOutputMixerCreation(current_reserved_physical_device());
}

// Removed listeners are not attached, and remaining listeners are.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStopCreate_TwoListeners) {
  MockOutputDeviceMixer* default_mixer = SetUpMockMixer_NoStreams();

  auto listener = GetListener_MixerExpectsStart(default_mixer);
  auto removed_listener = GetListener_MixerExpectsNoCalls(default_mixer);

  output_mixer_manager_.StartListening(listener.get(),
                                       current_default_physical_device());
  output_mixer_manager_.StartListening(removed_listener.get(),
                                       current_default_physical_device());
  output_mixer_manager_.StopListening(removed_listener.get());
  ForceOutputMixerCreation(current_default_physical_device());
}

TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_CreateStartStop_ReservedId) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  auto listener = GetListener_MixerExpectsStartStop(reserved_mixer);

  ForceOutputMixerCreation(reserved_device_id());
  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());
  output_mixer_manager_.StopListening(listener.get());
}

TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartCreateStop_ReservedId) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  auto listener = GetListener_MixerExpectsStartStop(reserved_mixer);

  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());
  ForceOutputMixerCreation(reserved_device_id());
  output_mixer_manager_.StopListening(listener.get());
}

TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartStopCreate_ReservedId) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  auto listener = GetListener_MixerExpectsNoCalls(reserved_mixer);

  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());
  output_mixer_manager_.StopListening(listener.get());
  ForceOutputMixerCreation(reserved_device_id());
}

TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_StartCreateStop_DefaultId_EmptyDeviceId) {
  MockOutputDeviceMixer* default_mixer = SetUpMockMixer_NoStreams();

  auto listener = GetListener_MixerExpectsStartStop(default_mixer);

  // kEmptyDeviceId should be treated the same as kReservedDefaultId.
  output_mixer_manager_.StartListening(listener.get(), kEmptyDeviceId);
  ForceOutputMixerCreation(kEmptyDeviceId);
  output_mixer_manager_.StopListening(listener.get());
}

// Makes sure reserved-listeners are attached to the reserved-mixer when it is
// created via current_reserved_physical_device().
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_ReservedListenersAttachToCurrentReservedIdMixer) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  auto listener = GetListener_MixerExpectsStartStop(reserved_mixer);

  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());
  ForceOutputMixerCreation(current_reserved_physical_device());
  output_mixer_manager_.StopListening(listener.get());
}

// Makes sure current_reserved_physical_device() listeners are attached when the
// reserved-mixer is created.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_CurrentReservedIdListenersAttachToReservedMixer) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  auto listener = GetListener_MixerExpectsStartStop(reserved_mixer);

  output_mixer_manager_.StartListening(listener.get(),
                                       current_reserved_physical_device());
  ForceOutputMixerCreation(reserved_device_id());
  output_mixer_manager_.StopListening(listener.get());
}

// Makes sure the presence of listeners does not force device recreation
// on device change.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_NoCreateAfterDeviceChange_WithListeners) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kOtherFakeDeviceId);

  // |mixer| should never get a call to StopListening(|listener|).
  auto listener = GetListener_MixerExpectsStart(mixer);

  ForceOutputMixerCreation(kOtherFakeDeviceId);
  output_mixer_manager_.StartListening(listener.get(), kOtherFakeDeviceId);

  SimulateDeviceChange();

  output_mixer_manager_.StopListening(listener.get());
}

// Makes sure listeners are re-attached when mixers are recreated.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_ListenersReattachedAfterDeviceChange) {
  MockOutputDeviceMixer* mixer = SetUpMockMixer_NoStreams(kOtherFakeDeviceId);

  // |mixer| should never get a call to StopListening(|listener|).
  auto listener = GetListener_MixerExpectsStart(mixer);

  ForceOutputMixerCreation(kOtherFakeDeviceId);
  output_mixer_manager_.StartListening(listener.get(), kOtherFakeDeviceId);

  SimulateDeviceChange();

  // Clear expectations so we can set up new ones.
  testing::Mock::VerifyAndClearExpectations(this);
  testing::Mock::VerifyAndClearExpectations(listener.get());

  // The same |listener| should be started when |new_mixer| is created.
  MockOutputDeviceMixer* new_mixer =
      SetUpMockMixer_NoStreams(kOtherFakeDeviceId);
  EXPECT_CALL(*new_mixer, StartListening(listener.get())).Times(1);

  ForceOutputMixerCreation(kOtherFakeDeviceId);
}

// Makes sure the reserved listeners are re-attached when mixers are
// re-created.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_ReservedIdListenersReattachedAfterDeviceChange) {
  SetAudioManagerReservedIdSupport(true);

  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  auto listener = GetListener_MixerExpectsStart(reserved_mixer);

  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());

  // |listener| will be started when |reserved_mixer| is created.
  ForceOutputMixerCreation(current_reserved_physical_device());

  // Make sure the AudioManager::GetDefaultOutputDeviceId() returns a new value.
  ASSERT_NE(current_reserved_physical_device(), kOtherFakeDeviceId);
  SimulateReservedDeviceChange(kOtherFakeDeviceId);

  testing::Mock::VerifyAndClearExpectations(this);
  testing::Mock::VerifyAndClearExpectations(listener.get());

  // |listener| should be attached to |new_default_mixer| when it is created.
  MockOutputDeviceMixer* new_reserved_mixer = SetUpReservedMixer_NoStreams();
  EXPECT_CALL(*new_reserved_mixer, StartListening(listener.get())).Times(1);

  ASSERT_EQ(kOtherFakeDeviceId, current_reserved_physical_device());
  ForceOutputMixerCreation(kOtherFakeDeviceId);
}

// Makes sure the reserved listeners are not attached to non-reserved listeners,
// if support for AudioManager's GetDefaultOutputDeviceID() or
// GetCommunicationsOutputDeviceID() changes.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_CurrentDefaultListenersNotReattached) {
  SetAudioManagerReservedIdSupport(true);

  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  // |reserved_mixer| should never get a call to StopListening(|listener|).
  auto listener = GetListener_MixerExpectsStart(reserved_mixer);

  output_mixer_manager_.StartListening(listener.get(),
                                       current_reserved_physical_device());

  // |listener| should be attached to |mixer|.
  ForceOutputMixerCreation(reserved_device_id());

  SetAudioManagerReservedIdSupport(false);
  SimulateDeviceChange();

  testing::Mock::VerifyAndClearExpectations(this);
  testing::Mock::VerifyAndClearExpectations(listener.get());

  // Now that AudioManager::GetDefaultOutputDeviceID() or
  // AudioManager::GetCommunicationsOutputDeviceID() only returns
  // kEmptyDeviceId, |listener| should not be attached to |new_reserved_mixer|.
  // Note: kReservedCommsId maps to kNormalizedDefaultDeviceId when
  //       |!audio_manager_supports_communications_physical_id_|.
  MockOutputDeviceMixer* new_reserved_mixer =
      SetUpMockMixer_NoStreams(kNormalizedDefaultDeviceId);
  EXPECT_CALL(*new_reserved_mixer, StartListening(listener.get())).Times(0);

  ForceOutputMixerCreation(reserved_device_id());

  testing::Mock::VerifyAndClearExpectations(this);
  testing::Mock::VerifyAndClearExpectations(listener.get());

  // |listener| should still be attached to |new_physical_mixer| when it's
  // created after a device change.
  MockOutputDeviceMixer* new_physical_mixer =
      SetUpMockMixer_NoStreams(current_reserved_physical_device());
  EXPECT_CALL(*new_physical_mixer, StartListening(listener.get())).Times(1);

  // |listener| should be attached to |new_physical_mixer|.
  ForceOutputMixerCreation(current_reserved_physical_device());
}

// Makes sure both "default listeners" and "current_reserved_physical_device()
// listeners" get attached to the same current_reserved_physical_device() mixer.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_CurrentDefaultMixerCreation_ListenersAttached) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  // Create listeners for reserved_device_id() and
  // current_reserved_physical_device(), BOTH listening to |reserved_mixer|.
  auto reserved_listener = GetListener_MixerExpectsStart(reserved_mixer);
  auto current_reserved_physical_listener =
      GetListener_MixerExpectsStart(reserved_mixer);

  // Create another listener, NOT listening to |reserved_mixer|.
  ASSERT_NE(kOtherFakeDeviceId, current_reserved_physical_device());
  auto other_listener = GetListener_MixerExpectsNoCalls(reserved_mixer);

  // Start all listeners.
  output_mixer_manager_.StartListening(reserved_listener.get(),
                                       reserved_device_id());
  output_mixer_manager_.StartListening(current_reserved_physical_listener.get(),
                                       current_reserved_physical_device());
  output_mixer_manager_.StartListening(other_listener.get(),
                                       kOtherFakeDeviceId);

  // |default_listener| and |current_default_physical_listener| should be
  // attached to |default_mixer|.
  ForceOutputMixerCreation(current_reserved_physical_device());
}

// Makes sure both "reserved listeners" and "current_reserve_physical_device()
// listeners" get attached to the same reserved mixer.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_ReservedIdMixerCreation_ListenersAttached) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();

  // Create listeners for reserved_device_id() and
  // current_reserved_physical_device(), BOTH listening to |reserved_mixer|.
  auto reserved_listener = GetListener_MixerExpectsStart(reserved_mixer);
  auto current_reserved_physical_listener =
      GetListener_MixerExpectsStart(reserved_mixer);

  // Create another listener, NOT listening to |reserved_mixer|.
  ASSERT_NE(kOtherFakeDeviceId, current_reserved_physical_device());
  auto other_listener = GetListener_MixerExpectsNoCalls(reserved_mixer);

  // Start all listeners.
  output_mixer_manager_.StartListening(reserved_listener.get(),
                                       reserved_device_id());
  output_mixer_manager_.StartListening(current_reserved_physical_listener.get(),
                                       current_reserved_physical_device());
  output_mixer_manager_.StartListening(other_listener.get(),
                                       kOtherFakeDeviceId);

  // |reserved_listener| and |current_reserved_physical_listener| should be
  // attached to |reserved_mixer|.
  ForceOutputMixerCreation(reserved_device_id());
}

// Makes sure both "reserved listeners" and "current_reserved_physical_device()
// listeners" don't get attached to non-reserved mixers.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_OtherDeviceMixerCreation_ListenersNotAttached) {
  MockOutputDeviceMixer* other_mixer =
      SetUpMockMixer_NoStreams(kOtherFakeDeviceId);

  // Create listeners for reserved_device_id() and
  // current_reserved_physical_device(), BOTH NOT listening to |other_mixer|.
  auto reserved_listener = GetListener_MixerExpectsNoCalls(other_mixer);
  auto current_reserved_physical_listener =
      GetListener_MixerExpectsNoCalls(other_mixer);

  // Create another listener, listening to |other_mixer|.
  ASSERT_NE(kOtherFakeDeviceId, current_reserved_physical_device());
  auto other_listener = GetListener_MixerExpectsStart(other_mixer);

  // Start all listeners.
  output_mixer_manager_.StartListening(reserved_listener.get(),
                                       reserved_device_id());
  output_mixer_manager_.StartListening(current_reserved_physical_listener.get(),
                                       current_reserved_physical_device());
  output_mixer_manager_.StartListening(other_listener.get(),
                                       kOtherFakeDeviceId);

  // Only |other_listener| should be attached to |other_mixer|.
  ForceOutputMixerCreation(kOtherFakeDeviceId);
}

// Makes sure we can call StartListening multiple times with the same listener,
// when the different device IDs map to the same mixer.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_MultipleStarts_EquivalentIds) {
  MockOutputDeviceMixer* default_mixer = SetUpReservedMixer_NoStreams();
  ForceOutputMixerCreation(reserved_device_id());

  auto listener = GetListener_MixerExpectsStartStop(default_mixer);

  // Start listener.
  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());

  // Verify starting with the same ID.
  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());

  // Verify starting with equivalent IDs.
  if (reserved_id_test_type() == ReservedIdTestType::kDefault) {
    // The kEmptyDeviceId also maps to kReservedDefaultId.
    output_mixer_manager_.StartListening(listener.get(), kEmptyDeviceId);
  }
  output_mixer_manager_.StartListening(listener.get(),
                                       current_reserved_physical_device());

  // Return to the original ID.
  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());

  output_mixer_manager_.StopListening(listener.get());
}

// Makes sure we can call StartListening multiple times with the same listener,
// with different device IDs.
TEST_F(OutputDeviceMixerManagerTest,
       DeviceOutputListener_MultipleStarts_DifferentIds) {
  MockOutputDeviceMixer* default_mixer;
  MockOutputDeviceMixer* other_mixer;
  {
    InSequence s;
    default_mixer = SetUpMockMixer_NoStreams(kNormalizedDefaultDeviceId);
    other_mixer = SetUpMockMixer_NoStreams(kOtherFakeDeviceId);
    ForceOutputMixerCreation(kReservedDefaultId);
    ForceOutputMixerCreation(kOtherFakeDeviceId);
  }

  auto listener = GetListener_MixerExpectsStartStop(default_mixer);
  EXPECT_CALL(*other_mixer, StartListening(listener.get())).Times(1);
  EXPECT_CALL(*other_mixer, StopListening(listener.get())).Times(0);

  output_mixer_manager_.StartListening(listener.get(), kReservedDefaultId);

  // This call should stop |default_mixer|.
  output_mixer_manager_.StartListening(listener.get(), kOtherFakeDeviceId);
}

// Makes sure listeners are properly updated internally when going from a
// reserved to a specific device.
TEST_P(OutputDeviceMixerManagerTest,
       DeviceOutputListener_MultipleStarts_ReservedToSpecific) {
  MockOutputDeviceMixer* reserved_mixer = SetUpReservedMixer_NoStreams();
  ForceOutputMixerCreation(reserved_device_id());
  testing::Mock::VerifyAndClearExpectations(this);

  std::string original_reserved_id = current_reserved_physical_device();

  auto listener = GetListener_MixerExpectsStart(reserved_mixer);
  output_mixer_manager_.StartListening(listener.get(), reserved_device_id());

  // Switch |listener| to listen to the current reserved device ID.
  output_mixer_manager_.StartListening(listener.get(), original_reserved_id);

  // Change the reserved device ID.
  ASSERT_NE(current_reserved_physical_device(), kOtherFakeDeviceId);
  SimulateReservedDeviceChange(kOtherFakeDeviceId);
  ASSERT_EQ(current_reserved_physical_device(), kOtherFakeDeviceId);

  // The reserved mixer should not receive any start/stop calls with listener.
  MockOutputDeviceMixer* new_reserved_mixer = SetUpReservedMixer_NoStreams();
  EXPECT_CALL(*new_reserved_mixer, StartListening(listener.get())).Times(0);
  EXPECT_CALL(*new_reserved_mixer, StopListening(listener.get())).Times(0);
  ForceOutputMixerCreation(reserved_device_id());
  testing::Mock::VerifyAndClearExpectations(this);

  // The |original_reserved_id| mixer should be started with listener.
  MockOutputDeviceMixer* physical_mixer =
      SetUpMockMixer_NoStreams(original_reserved_id);
  EXPECT_CALL(*physical_mixer, StartListening(listener.get())).Times(1);
  EXPECT_CALL(*physical_mixer, StopListening(listener.get())).Times(0);
  ForceOutputMixerCreation(original_reserved_id);
}

// Makes sure that there one shared default and communications mixer, if the
// current default and communication physical IDs are identical.
TEST_F(OutputDeviceMixerManagerTest,
       ReservedIds_DefaultAndCommunicationsPhysicalIdsShared) {
  SetAudioManagerDefaultIdSupport(true);
  SetAudioManagerCommunicationsIdSupport(true);

  // Set both the "default" and "communications" physical ID to be the same.
  SimulateDeviceChange(kFakeDeviceId, kFakeDeviceId);

  SetUpMockMixer_NoStreams(kNormalizedDefaultDeviceId);

  // All three IDs should resolve to a single kNormalizedDefaultDeviceId device.
  ForceOutputMixerCreation(kReservedDefaultId);
  ForceOutputMixerCreation(kReservedCommsId);
  ForceOutputMixerCreation(kFakeDeviceId);
}

// Makes sure that we map "communications" to kNormalizedDefaultDeviceId if the
// current physical communications device ID is empty.
TEST_F(OutputDeviceMixerManagerTest,
       ReservedIds_EmptyCommunicationsPhysicalId) {
  SetAudioManagerDefaultIdSupport(true);
  SetAudioManagerCommunicationsIdSupport(true);

  // Set the "communications" physical ID be empty.
  SimulateDeviceChange(kFakeDeviceId, kEmptyDeviceId);

  SetUpMockMixer_NoStreams(kNormalizedDefaultDeviceId);

  // The "communications" ID should resolve to kNormalizedDefaultDeviceId.
  ForceOutputMixerCreation(kReservedCommsId);
}

INSTANTIATE_TEST_SUITE_P(OutputDeviceMixerManagerTest,
                         OutputDeviceMixerManagerTest,
                         testing::Values(ReservedIdTestType::kDefault,
                                         ReservedIdTestType::kCommunications),
                         [](const ::testing::TestParamInfo<
                             OutputDeviceMixerManagerTest::ParamType>& info) {
                           switch (info.param) {
                             case ReservedIdTestType::kDefault:
                               return "Default";
                             case ReservedIdTestType::kCommunications:
                               return "Comms";
                           }
                         });

}  // namespace audio
