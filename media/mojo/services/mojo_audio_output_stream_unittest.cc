// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/test/task_environment.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const double kNewVolume = 0.618;
// Not actually used, but sent from the AudioOutputDelegate.
const int kStreamId = 0;
const int kShmemSize = 100;

using testing::_;
using testing::Mock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::Test;

class TestCancelableSyncSocket : public base::CancelableSyncSocket {
 public:
  TestCancelableSyncSocket() = default;

  void ExpectOwnershipTransfer() { expect_ownership_transfer_ = true; }

  ~TestCancelableSyncSocket() override {
    // When the handle is sent over mojo, mojo takes ownership over it and
    // closes it. We have to make sure we do not also retain the handle in the
    // sync socket, as the sync socket closes the handle on destruction.
    if (expect_ownership_transfer_)
      EXPECT_FALSE(IsValid());
  }

 private:
  bool expect_ownership_transfer_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestCancelableSyncSocket);
};

class MockDelegate : public AudioOutputDelegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD0(GetStreamId, int());
  MOCK_METHOD0(OnPlayStream, void());
  MOCK_METHOD0(OnPauseStream, void());
  MOCK_METHOD1(OnSetVolume, void(double));
  MOCK_METHOD0(OnFlushStream, void());
};

class MockDelegateFactory {
 public:
  void PrepareDelegateForCreation(
      std::unique_ptr<AudioOutputDelegate> delegate) {
    ASSERT_EQ(nullptr, delegate_);
    delegate_.swap(delegate);
  }

  std::unique_ptr<AudioOutputDelegate> CreateDelegate(
      AudioOutputDelegate::EventHandler* handler) {
    MockCreateDelegate(handler);
    EXPECT_NE(nullptr, delegate_);
    return std::move(delegate_);
  }

  MOCK_METHOD1(MockCreateDelegate, void(AudioOutputDelegate::EventHandler*));

 private:
  std::unique_ptr<AudioOutputDelegate> delegate_;
};

class MockDeleter {
 public:
  MOCK_METHOD1(Finished, void(bool));
};

class MockClient {
 public:
  MockClient() = default;

  void Initialize(mojom::ReadWriteAudioDataPipePtr data_pipe) {
    ASSERT_TRUE(data_pipe->shared_memory.IsValid());
    ASSERT_TRUE(data_pipe->socket.is_valid());

    socket_ = std::make_unique<base::CancelableSyncSocket>(
        data_pipe->socket.TakePlatformFile());
    EXPECT_TRUE(socket_->IsValid());

    shared_memory_region_ = std::move(data_pipe->shared_memory);

    GotNotification();
  }

  MOCK_METHOD0(GotNotification, void());

 private:
  base::UnsafeSharedMemoryRegion shared_memory_region_;
  std::unique_ptr<base::CancelableSyncSocket> socket_;
};

std::unique_ptr<AudioOutputDelegate> CreateNoDelegate(
    AudioOutputDelegate::EventHandler* event_handler) {
  return nullptr;
}

void NotCalled(mojo::PendingRemote<mojom::AudioOutputStream>,
               mojom::ReadWriteAudioDataPipePtr) {
  ADD_FAILURE() << "The StreamCreated callback was called despite the test "
                   "expecting it not to.";
}

}  // namespace

class MojoAudioOutputStreamTest : public Test {
 public:
  MojoAudioOutputStreamTest()
      : foreign_socket_(std::make_unique<TestCancelableSyncSocket>()) {}

  mojo::Remote<mojom::AudioOutputStream> CreateAudioOutput() {
    mojo::Remote<mojom::AudioOutputStream> remote;
    pending_stream_receiver = remote.BindNewPipeAndPassReceiver();
    ExpectDelegateCreation();
    impl_ = std::make_unique<MojoAudioOutputStream>(
        base::BindOnce(&MockDelegateFactory::CreateDelegate,
                       base::Unretained(&mock_delegate_factory_)),
        base::BindOnce(&MojoAudioOutputStreamTest::CreatedStream,
                       base::Unretained(this)),
        base::BindOnce(&MockDeleter::Finished, base::Unretained(&deleter_)));
    return remote;
  }

 protected:
  void CreatedStream(mojo::PendingRemote<mojom::AudioOutputStream> stream,
                     mojom::ReadWriteAudioDataPipePtr data_pipe) {
    EXPECT_EQ(mojo::FuseMessagePipes(pending_stream_receiver.PassPipe(),
                                     stream.PassPipe()),
              MOJO_RESULT_OK);
    client_.Initialize(std::move(data_pipe));
  }

  void ExpectDelegateCreation() {
    delegate_ = new StrictMock<MockDelegate>();
    mock_delegate_factory_.PrepareDelegateForCreation(
        base::WrapUnique(delegate_));
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&local_, foreign_socket_.get()));
    mem_ = base::UnsafeSharedMemoryRegion::Create(kShmemSize);
    EXPECT_TRUE(mem_.IsValid());
    EXPECT_CALL(mock_delegate_factory_, MockCreateDelegate(NotNull()))
        .WillOnce(SaveArg<0>(&delegate_event_handler_));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::CancelableSyncSocket local_;
  std::unique_ptr<TestCancelableSyncSocket> foreign_socket_;
  base::UnsafeSharedMemoryRegion mem_;
  StrictMock<MockDelegate>* delegate_ = nullptr;
  AudioOutputDelegate::EventHandler* delegate_event_handler_ = nullptr;
  StrictMock<MockDelegateFactory> mock_delegate_factory_;
  StrictMock<MockDeleter> deleter_;
  StrictMock<MockClient> client_;
  mojo::PendingReceiver<mojom::AudioOutputStream> pending_stream_receiver;
  std::unique_ptr<MojoAudioOutputStream> impl_;
};

TEST_F(MojoAudioOutputStreamTest, NoDelegate_SignalsError) {
  MojoAudioOutputStream stream(
      base::BindOnce(&CreateNoDelegate), base::BindOnce(&NotCalled),
      base::BindOnce(&MockDeleter::Finished, base::Unretained(&deleter_)));
  EXPECT_CALL(deleter_, Finished(true));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, Play_Plays) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();

  EXPECT_CALL(client_, GotNotification());
  EXPECT_CALL(*delegate_, OnPlayStream());

  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  audio_output_remote->Play();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, Pause_Pauses) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();

  EXPECT_CALL(client_, GotNotification());
  EXPECT_CALL(*delegate_, OnPauseStream());

  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  audio_output_remote->Pause();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, SetVolume_SetsVolume) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();

  EXPECT_CALL(client_, GotNotification());
  EXPECT_CALL(*delegate_, OnSetVolume(kNewVolume));

  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  audio_output_remote->SetVolume(kNewVolume);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, Flush_FlushesStream) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();

  EXPECT_CALL(client_, GotNotification());
  EXPECT_CALL(*delegate_, OnFlushStream());
  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  audio_output_remote->Flush();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, DestructWithCallPending_Safe) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();
  EXPECT_CALL(client_, GotNotification());
  base::RunLoop().RunUntilIdle();

  ASSERT_NE(nullptr, delegate_event_handler_);
  foreign_socket_->ExpectOwnershipTransfer();
  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  audio_output_remote->Play();
  impl_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, Created_NotifiesClient) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(client_, GotNotification());

  ASSERT_NE(nullptr, delegate_event_handler_);
  foreign_socket_->ExpectOwnershipTransfer();
  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));

  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoAudioOutputStreamTest, SetVolumeTooLarge_Error) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();
  EXPECT_CALL(deleter_, Finished(true));
  EXPECT_CALL(client_, GotNotification());

  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  audio_output_remote->SetVolume(15);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

TEST_F(MojoAudioOutputStreamTest, SetVolumeNegative_Error) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();
  EXPECT_CALL(deleter_, Finished(true));
  EXPECT_CALL(client_, GotNotification());

  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  audio_output_remote->SetVolume(-0.5);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

TEST_F(MojoAudioOutputStreamTest, DelegateErrorBeforeCreated_PropagatesError) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();
  EXPECT_CALL(deleter_, Finished(true));

  ASSERT_NE(nullptr, delegate_event_handler_);
  delegate_event_handler_->OnStreamError(kStreamId);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

TEST_F(MojoAudioOutputStreamTest, DelegateErrorAfterCreated_PropagatesError) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();
  EXPECT_CALL(client_, GotNotification());
  EXPECT_CALL(deleter_, Finished(true));
  base::RunLoop().RunUntilIdle();

  ASSERT_NE(nullptr, delegate_event_handler_);
  foreign_socket_->ExpectOwnershipTransfer();
  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  delegate_event_handler_->OnStreamError(kStreamId);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

TEST_F(MojoAudioOutputStreamTest, RemoteEndGone_CallsDeleter) {
  mojo::Remote<mojom::AudioOutputStream> audio_output_remote =
      CreateAudioOutput();

  EXPECT_CALL(client_, GotNotification());
  EXPECT_CALL(deleter_, Finished(false));

  delegate_event_handler_->OnStreamCreated(kStreamId, std::move(mem_),
                                           std::move(foreign_socket_));
  audio_output_remote.reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&deleter_);
}

}  // namespace media
