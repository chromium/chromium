// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

// Aliases for use with MockCreatedCallback::Created().
const bool successfully_ = true;
const bool unsuccessfully_ = false;

class MockStream : public media::AudioOutputStream {
 public:
  MockStream() {}

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(Start, void(AudioSourceCallback* callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Flush, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStream);
};

const uint32_t kPlatformErrorDisconnectReason = static_cast<uint32_t>(
    media::mojom::AudioOutputStreamObserver::DisconnectReason::kPlatformError);
const uint32_t kTerminatedByClientDisconnectReason =
    static_cast<uint32_t>(media::mojom::AudioOutputStreamObserver::
                              DisconnectReason::kTerminatedByClient);

class MockObserver : public media::mojom::AudioOutputStreamObserver {
 public:
  MockObserver() = default;

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

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class MockCreatedCallback {
 public:
  MockCreatedCallback() {}

  MOCK_METHOD1(Created, void(bool /*valid*/));

  void OnCreated(media::mojom::ReadWriteAudioDataPipePtr ptr) {
    Created(!!ptr);
  }

  OutputStream::CreatedCallback Get() {
    return base::BindOnce(&MockCreatedCallback::OnCreated,
                          base::Unretained(this));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCreatedCallback);
};

}  // namespace

// Instantiates various classes that we're going to want in most test cases.
class TestEnvironment {
 public:
  TestEnvironment()
      : audio_manager_(std::make_unique<media::TestAudioThread>(false)),
        stream_factory_(&audio_manager_),
        stream_factory_receiver_(
            &stream_factory_,
            remote_stream_factory_.BindNewPipeAndPassReceiver()) {
    mojo::core::SetDefaultProcessErrorCallback(bad_message_callback_.Get());
  }

  ~TestEnvironment() {
    audio_manager_.Shutdown();
    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
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
        base::UnguessableToken::Create(), base::nullopt,
        created_callback_.Get());
    return remote_stream;
  }

  mojo::PendingRemote<media::mojom::AudioOutputStream>
  CreateStreamWithNullptrObserver() {
    mojo::PendingRemote<media::mojom::AudioOutputStream> remote_stream;
    remote_stream_factory_->CreateOutputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(),
        mojo::NullAssociatedRemote(), log_.MakeRemote(), "",
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), base::nullopt,
        created_callback_.Get());
    return remote_stream;
  }

  mojo::PendingRemote<media::mojom::AudioOutputStream>
  CreateStreamWithNullptrLog() {
    mojo::PendingRemote<media::mojom::AudioOutputStream> remote_stream;
    remote_stream_factory_->CreateOutputStream(
        remote_stream.InitWithNewPipeAndPassReceiver(), observer_.MakeRemote(),
        mojo::NullRemote(), "",
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), base::nullopt,
        created_callback_.Get());
    return remote_stream;
  }

  media::MockAudioManager& audio_manager() { return audio_manager_; }

  MockObserver& observer() { return observer_; }

  MockLog& log() { return log_; }

  MockCreatedCallback& created_callback() { return created_callback_; }

  MockBadMessageCallback& bad_message_callback() {
    return bad_message_callback_;
  }

 private:
  base::test::TaskEnvironment tasks_;
  media::MockAudioManager audio_manager_;
  StreamFactory stream_factory_;
  mojo::Remote<mojom::StreamFactory> remote_stream_factory_;
  mojo::Receiver<mojom::StreamFactory> stream_factory_receiver_;
  StrictMock<MockObserver> observer_;
  NiceMock<MockLog> log_;
  StrictMock<MockCreatedCallback> created_callback_;
  StrictMock<MockBadMessageCallback> bad_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestEnvironment);
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

}  // namespace audio
