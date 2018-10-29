// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_stream.h"

#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
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
  MockObserver() : binding_(this) {}

  media::mojom::AudioOutputStreamObserverAssociatedPtrInfo MakePtrInfo() {
    DCHECK(!binding_.is_bound());
    media::mojom::AudioOutputStreamObserverAssociatedPtrInfo ptr_info;
    binding_.Bind(mojo::MakeRequest(&ptr_info));
    binding_.set_connection_error_with_reason_handler(base::BindOnce(
        &MockObserver::BindingConnectionError, base::Unretained(this)));
    return ptr_info;
  }

  void CloseBinding() { binding_.Close(); }

  MOCK_METHOD0(DidStartPlaying, void());
  MOCK_METHOD0(DidStopPlaying, void());
  MOCK_METHOD1(DidChangeAudibleState, void(bool));

  MOCK_METHOD2(BindingConnectionError,
               void(uint32_t /*disconnect_reason*/, const std::string&));

 private:
  mojo::AssociatedBinding<media::mojom::AudioOutputStreamObserver> binding_;

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
        stream_factory_binding_(&stream_factory_,
                                mojo::MakeRequest(&stream_factory_ptr_)) {
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

  media::mojom::AudioOutputStreamPtr CreateStream() {
    media::mojom::AudioOutputStreamPtr stream_ptr;
    stream_factory_ptr_->CreateOutputStream(
        mojo::MakeRequest(&stream_ptr), observer_.MakePtrInfo(), log_.MakePtr(),
        "", media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), base::nullopt,
        created_callback_.Get());
    return stream_ptr;
  }

  media::mojom::AudioOutputStreamPtr CreateStreamWithNullptrObserver() {
    media::mojom::AudioOutputStreamPtr stream_ptr;
    stream_factory_ptr_->CreateOutputStream(
        mojo::MakeRequest(&stream_ptr), nullptr, log_.MakePtr(), "",
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), base::nullopt,
        created_callback_.Get());
    return stream_ptr;
  }

  media::mojom::AudioOutputStreamPtr CreateStreamWithNullptrLog() {
    media::mojom::AudioOutputStreamPtr stream_ptr;
    stream_factory_ptr_->CreateOutputStream(
        mojo::MakeRequest(&stream_ptr), observer_.MakePtrInfo(), nullptr, "",
        media::AudioParameters::UnavailableDeviceParams(),
        base::UnguessableToken::Create(), base::nullopt,
        created_callback_.Get());
    return stream_ptr;
  }

  media::MockAudioManager& audio_manager() { return audio_manager_; }

  MockObserver& observer() { return observer_; }

  MockLog& log() { return log_; }

  MockCreatedCallback& created_callback() { return created_callback_; }

  MockBadMessageCallback& bad_message_callback() {
    return bad_message_callback_;
  }

 private:
  base::test::ScopedTaskEnvironment tasks_;
  media::MockAudioManager audio_manager_;
  StreamFactory stream_factory_;
  mojom::StreamFactoryPtr stream_factory_ptr_;
  mojo::Binding<mojom::StreamFactory> stream_factory_binding_;
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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(env.log(), OnClosed());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream_ptr.reset();
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

  media::mojom::AudioOutputStreamPtr stream_ptr =
      env.CreateStreamWithNullptrObserver();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(env.log(), OnClosed());
  EXPECT_CALL(mock_stream, Close());
  stream_ptr.reset();
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

  media::mojom::AudioOutputStreamPtr stream_ptr =
      env.CreateStreamWithNullptrLog();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream_ptr.reset();
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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));

  stream_ptr.reset();
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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Start(NotNull()));
  EXPECT_CALL(env.log(), OnStarted());
  EXPECT_CALL(env.observer(), DidStartPlaying());
  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));
  stream_ptr->Play();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  EXPECT_CALL(mock_stream, Stop());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(), DidChangeAudibleState(false)).Times(AtMost(1));
  EXPECT_CALL(env.observer(), DidStopPlaying()).Times(AtMost(1));
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream_ptr.reset();
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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Start(NotNull()));
  EXPECT_CALL(env.observer(), DidStartPlaying());
  // May or may not get an audibility notification depending on if power
  // monitoring is enabled.
  EXPECT_CALL(env.observer(), DidChangeAudibleState(true)).Times(AtMost(1));
  stream_ptr->Play();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  EXPECT_CALL(mock_stream, Stop());
  EXPECT_CALL(env.log(), OnStopped());
  EXPECT_CALL(env.observer(), DidChangeAudibleState(false)).Times(AtMost(1));
  EXPECT_CALL(env.observer(), DidStopPlaying());
  stream_ptr->Pause();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.observer());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream_ptr.reset();
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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, SetVolume(new_volume));
  EXPECT_CALL(env.log(), OnSetVolume(new_volume));
  stream_ptr->SetVolume(new_volume);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kTerminatedByClientDisconnectReason, _));
  stream_ptr.reset();
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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kPlatformErrorDisconnectReason, _));
  EXPECT_CALL(env.bad_message_callback(), Run(_));
  stream_ptr->SetVolume(-0.1);
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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&mock_stream);
  Mock::VerifyAndClear(&env.created_callback());

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(env.observer(),
              BindingConnectionError(kPlatformErrorDisconnectReason, _));
  EXPECT_CALL(env.bad_message_callback(), Run(_));
  stream_ptr->SetVolume(1.1);
  base::RunLoop().RunUntilIdle();
}

TEST(AudioServiceOutputStreamTest,
     ConstructWithStreamCreationFailure_SignalsError) {
  TestEnvironment env;

  // By default, the MockAudioManager fails to create a stream.

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();

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

  media::mojom::AudioOutputStreamPtr stream_ptr = env.CreateStream();
  EXPECT_CALL(env.created_callback(), Created(unsuccessfully_));

  EXPECT_CALL(env.observer(),
              BindingConnectionError(kPlatformErrorDisconnectReason, _));

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&env.observer());
}

}  // namespace audio
