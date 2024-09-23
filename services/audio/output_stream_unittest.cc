// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_stream.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_power_monitor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/audio/stream_factory.h"
#include "services/audio/test/mock_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtMost;
using testing::DeleteArg;
using testing::Mock;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace audio {

namespace {

constexpr char kDeviceId1[] = "device 1";
constexpr char kDeviceId2[] = "device 2";

// Aliases for use with MockCreatedCallback::Created().
const bool successfully_ = true;
const bool unsuccessfully_ = false;

class MockStream : public media::AudioOutputStream {
 public:
  MockStream() {}

  MockStream(const MockStream&) = delete;
  MockStream& operator=(const MockStream&) = delete;

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(Start, void(AudioSourceCallback* callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());
};

const uint32_t kPlatformErrorDisconnectReason = static_cast<uint32_t>(
    media::mojom::AudioOutputStreamObserver::DisconnectReason::kPlatformError);
const uint32_t kTerminatedByClientDisconnectReason =
    static_cast<uint32_t>(media::mojom::AudioOutputStreamObserver::
                              DisconnectReason::kTerminatedByClient);

class MockObserver : public media::mojom::AudioOutputStreamObserver {
 public:
  MockObserver() = default;

  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;

  mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
  MakeRemote() {
    DCHECK(!receiver_.is_bound());
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        remote;
    receiver_.Bind(remote.InitWithNewEndpointAndPassReceiver());
    receiver_.set_disconnect_with_reason_handler(base::BindOnce(
        &MockObserver::BindingConnectionError, base::Unretained(this)));
    return remote;
  }

  void CloseBinding() { receiver_.reset(); }

  MOCK_METHOD0(DidStartPlaying, void());
  MOCK_METHOD0(DidStopPlaying, void());
  MOCK_METHOD1(DidChangeAudibleState, void(bool));

  MOCK_METHOD2(BindingConnectionError,
               void(uint32_t /*disconnect_reason*/, const std::string&));

 private:
  mojo::AssociatedReceiver<media::mojom::AudioOutputStreamObserver> receiver_{
      this};
};

class MockCreatedCallback {
 public:
  MockCreatedCallback() {}

  MockCreatedCallback(const MockCreatedCallback&) = delete;
  MockCreatedCallback& operator=(const MockCreatedCallback&) = delete;

  MOCK_METHOD1(Created, void(bool /*valid*/));

  void OnCreated(media::mojom::ReadWriteAudioDataPipePtr ptr) {
    Created(!!ptr);
  }

  OutputStream::CreatedCallback Get() {
    return base::BindOnce(&MockCreatedCallback::OnCreated,
                          base::Unretained(this));
  }
};

class MockStreamFactory {
 public:
  MOCK_METHOD(media::AudioOutputStream*,
              CreateStream,
              (const media::AudioParameters&, const std::string&));
};

}  // namespace

// Instantiates various classes that we're going to want in most test cases.
class TestEnvironment {
 public:
  TestEnvironment()
      : audio_manager_(std::make_unique<media::TestAudioThread>(false)),
        stream_factory_(&audio_manager_, /*aecdump_recording_manager=*/nullptr),
        stream_factory_receiver_(
            &stream_factory_,
            remote_stream_factory_.BindNewPipeAndPassReceiver()) {
    mojo::SetDefaultProcessErrorHandler(bad_message_callback_.Get());
  }

  TestEnvironment(const TestEnvironment&) = delete;
  TestEnvironment& operator=(const TestEnvironment&) = delete;

  ~TestEnvironment() {
    audio_manager_.Shutdown();
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  using MockDeleteCallback = base::MockCallback<OutputStream::DeleteCallback>;
  using MockBadMessageCallback =
      base::MockCallback<base::RepeatingCallback<void(const std::string&)>>;

  mojo::PendingRemote<media::mojom::AudioOutputStream> CreateStream() {
    mojo::PendingRemote<media::mojom::AudioOutputStream> remote_stream;
    remote_stream_factory_->CreateOutputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(), observer_.MakeRemote(),
        log_.MakeRemote(), "",
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), created_callback_.Get());
    return remote_stream;
  }

  mojo::PendingRemote<media::mojom::AudioOutputStream>
  CreateStreamWithNullptrObserver() {
    mojo::PendingRemote<media::mojom::AudioOutputStream> remote_stream;
    remote_stream_factory_->CreateOutputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(),
        mojo::NullAssociatedRemote(), log_.MakeRemote(), "",
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), created_callback_.Get());
    return remote_stream;
  }

  mojo::PendingRemote<media::mojom::AudioOutputStream>
  CreateStreamWithNullptrLog() {
    mojo::PendingRemote<media::mojom::AudioOutputStream> remote_stream;
    remote_stream_factory_->CreateOutputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(), observer_.MakeRemote(),
        mojo::NullRemote(), "",
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), created_callback_.Get());
    return remote_stream;
  }

  mojo::PendingRemote<media::mojom::AudioOutputStream> CreateSwitchableStream(
      const std::string& device_id) {
    mojo::PendingRemote<media::mojom::AudioOutputStream> remote_stream;
    remote_stream_factory_->CreateSwitchableOutputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(),
        device_switch_interface_.BindNewPipeAndPassReceiver(),
        observer_.MakeRemote(), log_.MakeRemote(), device_id,
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), created_callback_.Get());
    return remote_stream;
  }

  media::MockAudioManager& audio_manager() { return audio_manager_; }

  MockObserver& observer() { return observer_; }

  MockLog& log() { return log_; }

  MockCreatedCallback& created_callback() { return created_callback_; }

  MockBadMessageCallback& bad_message_callback() {
    return bad_message_callback_;
  }

  media::mojom::AudioStreamFactory* remote_stream_factory() {
    return remote_stream_factory_.get();
  }

  media::mojom::DeviceSwitchInterface* device_switch_interface() {
    return device_switch_interface_.get();
  }

 private:
  base::test::TaskEnvironment tasks_;
  media::MockAudioManager audio_manager_;
  StreamFactory stream_factory_;
  mojo::Remote<media::mojom::AudioStreamFactory> remote_stream_factory_;
  mojo::Remote<media::mojom::DeviceSwitchInterface> device_switch_interface_;
  mojo::Receiver<media::mojom::AudioStreamFactory> stream_factory_receiver_;
  StrictMock<MockObserver> observer_;
  NiceMock<MockLog> log_;
  StrictMock<MockCreatedCallback> created_callback_;
  StrictMock<MockBadMessageCallback> bad_message_callback_;
};

TEST(AudioServiceOutputStreamTest, ConstructDestruct) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));
  EXPECT_CALL(env.log(), OnCreated(_, _));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(env.log(), OnClosed());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest, ConstructDestructNullptrObserver) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));
  EXPECT_CALL(env.log(), OnCreated(_, _));

  mojo::Remote<media::mojom::AudioOutputStream> stream(
      env.CreateStreamWithNullptrObserver());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(env.log(), OnClosed());
  EXPECT_CALL(mock_stream, Close());
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest, ConstructDestructNullptrLog) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(
      env.CreateStreamWithNullptrLog());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest,
     ConstructStreamAndDestructObserver_DestructsStream) {
  TestEnvironment env;
  MockStream mock_stream;
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(env.created_callback(), Created(successfully_));
  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());

  env.observer().CloseBinding();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClear(&mock_stream);
}

TEST(AudioServiceOutputStreamTest,
     ConstructStreamAndReleaseStreamPtr_DestructsStream) {
  TestEnvironment env;
  MockStream mock_stream;
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(env.created_callback(), Created(successfully_));
  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));

  stream.reset();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());
}

TEST(AudioServiceOutputStreamTest, Play_Plays) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Start(NotNull()));
  EXPECT_CALL(env.log(), OnStarted());
  EXPECT_CALL(env.observer(), DidStartPlaying());

  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));
  stream->Play();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  EXPECT_CALL(mock_stream, Stop());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(), DidChangeAudibleState(false)).Times(AtMost(1));
  EXPECT_CALL(env.observer(), DidStopPlaying()).Times(AtMost(1));
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest, PlayAndPause_PlaysAndStops) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Start(NotNull()));
  EXPECT_CALL(env.observer(), DidStartPlaying());

  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));
  stream->Play();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  EXPECT_CALL(mock_stream, Stop());
  EXPECT_CALL(env.log(), OnStopped());
  EXPECT_CALL(env.observer(), DidChangeAudibleState(false)).Times(AtMost(1));
  EXPECT_CALL(env.observer(), DidStopPlaying());
  stream->Pause();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest, SetVolume_SetsVolume) {
  double new_volume = 0.618;
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, SetVolume(new_volume));
  EXPECT_CALL(env.log(), OnSetVolume(new_volume));
  stream->SetVolume(new_volume);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest, SetNegativeVolume_BadMessage) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kPlatformErrorDisconnectReason, _));
  EXPECT_CALL(env.bad_message_callback(), Run(_));
  stream->SetVolume(-0.1);
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest, SetVolumeGreaterThanOne_BadMessage) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kPlatformErrorDisconnectReason, _));
  EXPECT_CALL(env.bad_message_callback(), Run(_));
  stream->SetVolume(1.1);
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest,
     ConstructWithStreamCreationFailure_SignalsError) {
  TestEnvironment env;

  // By default, the MockAudioManager fails to create a stream.

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());

  EXPECT_CALL(env.created_callback(), Created(unsuccessfully_));
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kPlatformErrorDisconnectReason, _));
  EXPECT_CALL(env.log(), OnError());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&env.observer());
}

TEST(AudioServiceOutputStreamTest,
     ConstructWithStreamCreationFailureAndDestructBeforeErrorFires_NoCrash) {
  // The main purpose of this test is to make sure that that delete callback
  // call is deferred, and that it is canceled in case of destruction.
  TestEnvironment env;

  // By default, the MockAudioManager fails to create a stream.

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  EXPECT_CALL(env.created_callback(), Created(unsuccessfully_));

  EXPECT_CALL(env.observer(),
              BindingConnectionError(kPlatformErrorDisconnectReason, _));

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&env.observer());
}

TEST(AudioServiceOutputStreamTest, BindMuters) {
  // Set up the test environment.
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));
  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      [](media::AudioOutputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));
  EXPECT_CALL(env.log(), OnCreated(_, _));

  mojo::Remote<media::mojom::AudioOutputStream> stream(env.CreateStream());
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  // Bind the first muter.
  mojo::AssociatedRemote<media::mojom::LocalMuter> muter1;
  base::UnguessableToken group_id = base::UnguessableToken::Create();
  env.remote_stream_factory()->BindMuter(
      muter1.BindNewEndpointAndPassReceiver(), group_id);
  base::RunLoop().RunUntilIdle();

  // Unbind the first muter and immediately bind the second muter. The muter
  // should not be destroyed in this case.
  muter1.reset();
  mojo::AssociatedRemote<media::mojom::LocalMuter> muter2;
  env.remote_stream_factory()->BindMuter(
      muter2.BindNewEndpointAndPassReceiver(), group_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(env.log(), OnClosed());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest, CreateSwitchableStreamAndPlay) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));

  MockStreamFactory mock_stream_factory;

  EXPECT_CALL(mock_stream_factory, CreateStream(_, kDeviceId1))
      .Times(1)
      .WillOnce(Return(&mock_stream));

  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      base::BindRepeating(&MockStreamFactory::CreateStream,
                          base::Unretained(&mock_stream_factory))));

  // SwitchAudioOutputDeviceId will reopen and the setVolume as well.
  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(
      env.CreateSwitchableStream(kDeviceId1));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Start(NotNull())).Times(1);
  EXPECT_CALL(env.log(), OnStarted()).Times(1);
  EXPECT_CALL(env.observer(), DidStartPlaying()).Times(1);

  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));
  stream->Play();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest,
     CreateSwitchableStreamPlayPauseSwitchDeviceId) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));

  MockStreamFactory mock_stream_factory;

  EXPECT_CALL(mock_stream_factory, CreateStream(_, kDeviceId1))
      .Times(1)
      .WillOnce(Return(&mock_stream));

  EXPECT_CALL(mock_stream_factory, CreateStream(_, kDeviceId2))
      .Times(1)
      .WillOnce(Return(&mock_stream));

  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      base::BindRepeating(&MockStreamFactory::CreateStream,
                          base::Unretained(&mock_stream_factory))));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(
      env.CreateSwitchableStream(kDeviceId1));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  // Play.
  EXPECT_CALL(mock_stream, Start(NotNull()));
  EXPECT_CALL(env.observer(), DidStartPlaying());
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));

  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));
  stream->Play();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  // Pause.
  EXPECT_CALL(mock_stream, Stop());
  EXPECT_CALL(env.log(), OnStopped());
  EXPECT_CALL(env.observer(), DidChangeAudibleState(false)).Times(AtMost(1));
  EXPECT_CALL(env.observer(), DidStopPlaying());
  stream->Pause();

  // SwitchDeviceId.
  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  // The current state is `stopped` so it does not trigger
  // `DidStartPlaying` or `Start`.
  EXPECT_CALL(env.observer(), DidStartPlaying()).Times(0);
  EXPECT_CALL(mock_stream, Start(NotNull())).Times(0);

  env.device_switch_interface()->SwitchAudioOutputDeviceId(kDeviceId2);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);

  // Play.
  EXPECT_CALL(mock_stream, Start(NotNull()));
  EXPECT_CALL(env.observer(), DidStartPlaying());
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));

  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));
  stream->Play();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest, CreateSwitchableStreamPlaySwitchDeviceId) {
  TestEnvironment env;
  MockStream mock_stream;
  EXPECT_CALL(env.created_callback(), Created(successfully_));

  MockStreamFactory mock_stream_factory;

  EXPECT_CALL(mock_stream_factory, CreateStream(_, kDeviceId1))
      .Times(1)
      .WillOnce(Return(&mock_stream));

  EXPECT_CALL(mock_stream_factory, CreateStream(_, kDeviceId2))
      .Times(1)
      .WillOnce(Return(&mock_stream));

  env.audio_manager().SetMakeOutputStreamCB(base::BindRepeating(
      base::BindRepeating(&MockStreamFactory::CreateStream,
                          base::Unretained(&mock_stream_factory))));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));

  mojo::Remote<media::mojom::AudioOutputStream> stream(
      env.CreateSwitchableStream(kDeviceId1));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  // Play.
  EXPECT_CALL(mock_stream, Start(NotNull())).Times(1);
  EXPECT_CALL(env.log(), OnStarted()).Times(1);
  EXPECT_CALL(env.observer(), DidStartPlaying()).Times(1);

  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));
  stream->Play();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  // SwitchDeviceId.
  // Switch the device id will trigger Open and SetVolume event like
  // the first stream is created.
  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetVolume(1));
  EXPECT_CALL(mock_stream, Start(NotNull()));

  // It already `play` state so it does not trigger `DidStartPlaying` event.
  EXPECT_CALL(env.observer(), DidStartPlaying()).Times(0);
  env.device_switch_interface()->SwitchAudioOutputDeviceId(kDeviceId2);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);

  // The AudioOutputStream commands continue to work.
  double new_volume = 0.712;
  EXPECT_CALL(mock_stream, SetVolume(new_volume));
  EXPECT_CALL(env.log(), OnSetVolume(new_volume));
  stream->SetVolume(new_volume);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

class OutputStreamAudibilityHelperTest : public ::testing::Test {
 public:
  OutputStreamAudibilityHelperTest()
      : helper_(OutputStream::MakeAudibilityHelperForTest()) {}

  MOCK_METHOD0(GetPowerLevel, float());
  MOCK_METHOD1(OnAudiblityStateChanged, void(bool));

  void VerifyAndClear() { testing::Mock::VerifyAndClear(this); }

  void StartWithMocks() {
    helper_->StartPolling(
        base::BindRepeating(&OutputStreamAudibilityHelperTest::GetPowerLevel,
                            base::Unretained(this)),
        base::BindRepeating(
            &OutputStreamAudibilityHelperTest::OnAudiblityStateChanged,
            base::Unretained(this)));
  }

  void Stop() { helper_->StopPolling(); }

  bool IsAudible() { return helper_->IsAudible(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<OutputStream::AudibilityHelper> helper_;
};

TEST_F(OutputStreamAudibilityHelperTest, StartsSilent) {
  EXPECT_FALSE(IsAudible());
}

TEST_F(OutputStreamAudibilityHelperTest, StartStop) {
  StartWithMocks();
  Stop();
}

TEST_F(OutputStreamAudibilityHelperTest, StartStopStop) {
  StartWithMocks();
  Stop();
  Stop();
}

TEST_F(OutputStreamAudibilityHelperTest, Stop) {
  Stop();
}

TEST_F(OutputStreamAudibilityHelperTest, Poll_Stop_StopsCallbacks) {
  EXPECT_CALL(*this, GetPowerLevel())
      .WillRepeatedly(Return(media::AudioPowerMonitor::zero_power()));

  StartWithMocks();
  task_environment_.FastForwardBy(base::Seconds(1));
  Stop();

  // Make sure we don't receive callbacks after stopping.
  VerifyAndClear();
  EXPECT_CALL(*this, GetPowerLevel()).Times(0);
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_F(OutputStreamAudibilityHelperTest, Poll_NeverAudible_NoStateChange) {
  EXPECT_CALL(*this, OnAudiblityStateChanged(testing::_)).Times(0);
  EXPECT_CALL(*this, GetPowerLevel())
      .WillRepeatedly(Return(media::AudioPowerMonitor::zero_power()));

  StartWithMocks();

  task_environment_.FastForwardBy(base::Seconds(1));

  Stop();
}

TEST_F(OutputStreamAudibilityHelperTest, Poll_Audible_StateChanges) {
  EXPECT_CALL(*this, OnAudiblityStateChanged(true)).Times(1);
  EXPECT_CALL(*this, GetPowerLevel())
      .WillOnce(Return(media::AudioPowerMonitor::max_power()));

  StartWithMocks();

  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());

  VerifyAndClear();

  // Expect a return to silence when stopping.
  EXPECT_CALL(*this, OnAudiblityStateChanged(false)).Times(1);
  Stop();
  EXPECT_FALSE(IsAudible());
}

// Makes sure that starting and stopping multiple time behaves as expected:
// - The helper should be silent on stops and re-starts
// - The first aubible audio should trigger the audibility change.
TEST_F(OutputStreamAudibilityHelperTest, Poll_StartStopTwice_StateChanges) {
  EXPECT_CALL(*this, OnAudiblityStateChanged(true)).Times(1);
  EXPECT_CALL(*this, OnAudiblityStateChanged(false)).Times(1);
  EXPECT_CALL(*this, GetPowerLevel())
      .WillRepeatedly(Return(media::AudioPowerMonitor::max_power()));

  // Start and stop a loud stream.
  StartWithMocks();
  task_environment_.FastForwardBy(base::Seconds(1));
  Stop();

  VerifyAndClear();
  EXPECT_FALSE(IsAudible());

  // Re-start, with a silent stream.
  EXPECT_CALL(*this, OnAudiblityStateChanged(true)).Times(0);
  EXPECT_CALL(*this, OnAudiblityStateChanged(false)).Times(0);
  EXPECT_CALL(*this, GetPowerLevel())
      .WillRepeatedly(Return(media::AudioPowerMonitor::zero_power()));

  StartWithMocks();

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsAudible());

  VerifyAndClear();

  // Make the stream loud again.
  EXPECT_CALL(*this, OnAudiblityStateChanged(true)).Times(1);
  EXPECT_CALL(*this, OnAudiblityStateChanged(false)).Times(1);
  EXPECT_CALL(*this, GetPowerLevel())
      .WillOnce(Return(media::AudioPowerMonitor::max_power()));

  // A single poll of the stream levels should transition to the audible state.
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  EXPECT_TRUE(IsAudible());

  Stop();
}

TEST_F(OutputStreamAudibilityHelperTest, Poll_AlternatingAudible_StateChanges) {
  StartWithMocks();

  constexpr int kIterations = 5;
  for (int i = 0; i < kIterations; ++i) {
    // First return loud levels and expect a transition to an audible state.
    VerifyAndClear();
    EXPECT_CALL(*this, GetPowerLevel())
        .WillRepeatedly(Return(media::AudioPowerMonitor::max_power()));
    EXPECT_CALL(*this, OnAudiblityStateChanged(true)).Times(1);

    task_environment_.FastForwardBy(base::Seconds(1));

    // Then return quiet levels and expect a transition to an silence state.
    VerifyAndClear();
    EXPECT_CALL(*this, OnAudiblityStateChanged(false)).Times(1);
    EXPECT_CALL(*this, GetPowerLevel())
        .WillRepeatedly(Return(media::AudioPowerMonitor::zero_power()));

    task_environment_.FastForwardBy(base::Seconds(1));
  }

  Stop();
}

TEST_F(OutputStreamAudibilityHelperTest, Poll_SmallGlitches_StaysAudible) {
  // Start the helper, force it into an audible state.
  EXPECT_CALL(*this, GetPowerLevel())
      .WillRepeatedly(Return(media::AudioPowerMonitor::max_power()));

  StartWithMocks();
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  VerifyAndClear();
  EXPECT_TRUE(IsAudible());

  // Simulate longer and longer glitches, making sure that the audibility helper
  // doesn't change state for small glitches.

  // These values are taken directly from AudibilityHelper's implementation,
  // in output_stream.cc.
  const base::TimeDelta kGlitchTolerance = base::Milliseconds(100);
  const base::TimeDelta kPollingPeriod = base::Seconds(1) / 15;

  base::TimeDelta glitch_length;
  std::optional<base::TimeTicks> glitch_start;
  const auto return_glitch_power_levels = [&glitch_length, &glitch_start]() {
    const base::TimeTicks now = base::TimeTicks::Now();

    // This is the first call to this lambda. Start a new glitch.
    if (!glitch_start) {
      glitch_start = now;
    }

    if (now - glitch_start.value() <= glitch_length) {
      return media::AudioPowerMonitor::zero_power();
    }

    return media::AudioPowerMonitor::max_power();
  };

  constexpr int kIterations = 5;
  for (int i = 0; i < kIterations; ++i) {
    // Force `return_glitch_power_levels` to return silent audio power levels.
    glitch_start = std::nullopt;
    glitch_length = (i + 1) * kPollingPeriod;

    EXPECT_CALL(*this, GetPowerLevel())
        .WillRepeatedly(return_glitch_power_levels);

    if (glitch_length < kGlitchTolerance) {
      // Expect to stay audible for a small glitch.
      EXPECT_CALL(*this, OnAudiblityStateChanged(false)).Times(0);
      EXPECT_CALL(*this, OnAudiblityStateChanged(true)).Times(0);
    } else {
      // Expect to become silent, then audible for a longer glitch.
      EXPECT_CALL(*this, OnAudiblityStateChanged(false)).Times(1);
      EXPECT_CALL(*this, OnAudiblityStateChanged(true)).Times(1);
    }

    task_environment_.FastForwardBy(base::Seconds(1));
    VerifyAndClear();
  }

  // Expect a return to silence when stopping.
  EXPECT_CALL(*this, OnAudiblityStateChanged(false)).Times(1);
  Stop();
}

}  // namespace audio
