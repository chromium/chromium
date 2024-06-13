// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_stream_handle.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_input_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using testing::StrictMock;
using testing::Mock;
using testing::Test;

class FakeAudioInputDelegate : public media::AudioInputDelegate {
 public:
  FakeAudioInputDelegate() {}

  FakeAudioInputDelegate(const FakeAudioInputDelegate&) = delete;
  FakeAudioInputDelegate& operator=(const FakeAudioInputDelegate&) = delete;

  ~FakeAudioInputDelegate() override {}

  int GetStreamId() override { return 0; }
  void OnRecordStream() override {}
  void OnSetVolume(double volume) override {}
  void OnSetOutputDeviceForAec(const std::string& output_device_id) override {}
};

class MockRendererAudioInputStreamFactoryClient
    : public blink::mojom::RendererAudioInputStreamFactoryClient {
 public:
  MOCK_METHOD0(Created, void());

  void StreamCreated(
      mojo::PendingRemote<media::mojom::AudioInputStream> input_stream,
      mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
          client_receiver,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const std::optional<base::UnguessableToken>& stream_id) override {
    EXPECT_TRUE(stream_id.has_value());
    input_stream_.Bind(std::move(input_stream));
    client_receiver_ = std::move(client_receiver);
    Created();
  }

 private:
  mojo::Remote<media::mojom::AudioInputStream> input_stream_;
  mojo::PendingReceiver<media::mojom::AudioInputStreamClient> client_receiver_;
};

using MockDeleter =
    base::MockCallback<base::OnceCallback<void(AudioInputStreamHandle*)>>;

// Creates a fake delegate and saves the provided event handler in
// |event_handler_out|.
std::unique_ptr<media::AudioInputDelegate> CreateFakeDelegate(
    raw_ptr<media::AudioInputDelegate::EventHandler>* event_handler_out,
    media::AudioInputDelegate::EventHandler* event_handler) {
  *event_handler_out = event_handler;
  return std::make_unique<FakeAudioInputDelegate>();
}

}  // namespace

class AudioInputStreamHandleTest : public Test {
 public:
  AudioInputStreamHandleTest()
      : client_receiver_(
            &client_,
            client_pending_remote_.InitWithNewPipeAndPassReceiver()),
        local_(std::make_unique<base::CancelableSyncSocket>()),
        remote_(std::make_unique<base::CancelableSyncSocket>()) {
    // Will set `event_handler_`.
    handle_ = std::make_unique<AudioInputStreamHandle>(
        std::move(client_pending_remote_),
        base::BindOnce(&CreateFakeDelegate, &event_handler_), deleter_.Get());

    // Wait for |event_handler| to be set.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(event_handler_);

    const size_t kSize = 1234;
    shared_memory_region_ =
        base::ReadOnlySharedMemoryRegion::Create(kSize).region;
    EXPECT_TRUE(shared_memory_region_.IsValid());
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(local_.get(), remote_.get()));
  }

  void SendCreatedNotification() {
    const int kIrrelevantStreamId = 0;
    const bool kInitiallyMuted = false;
    event_handler_->OnStreamCreated(kIrrelevantStreamId,
                                    std::move(shared_memory_region_),
                                    std::move(remote_), kInitiallyMuted);
  }

  MockRendererAudioInputStreamFactoryClient* client() { return &client_; }

  void ResetClientReceiver() { client_receiver_.reset(); }

  void ExpectHandleWillCallDeleter() {
    EXPECT_CALL(deleter_, Run(handle_.release()))
        .WillOnce([&](AudioInputStreamHandle* handle) {
          event_handler_ = nullptr;
          delete handle;
        });
  }

  // Note: Must call ExpectHandleWillCallDeleter() first.
  void VerifyDeleterWasCalled() {
    EXPECT_TRUE(Mock::VerifyAndClear(&deleter_));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<MockRendererAudioInputStreamFactoryClient> client_;
  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_pending_remote_;
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient>
      client_receiver_;
  StrictMock<MockDeleter> deleter_;
  std::unique_ptr<AudioInputStreamHandle> handle_;
  raw_ptr<media::AudioInputDelegate::EventHandler> event_handler_ = nullptr;

  base::ReadOnlySharedMemoryRegion shared_memory_region_;
  std::unique_ptr<base::CancelableSyncSocket> local_;
  std::unique_ptr<base::CancelableSyncSocket> remote_;
};

TEST_F(AudioInputStreamHandleTest, CreateStream) {
  EXPECT_CALL(*client(), Created());

  SendCreatedNotification();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(Mock::VerifyAndClear(client()));
}

TEST_F(AudioInputStreamHandleTest,
       DestructClientBeforeCreationFinishes_CancelsStreamCreation) {
  ExpectHandleWillCallDeleter();

  ResetClientReceiver();
  base::RunLoop().RunUntilIdle();

  VerifyDeleterWasCalled();
}

TEST_F(AudioInputStreamHandleTest,
       CreateStreamAndDisconnectClient_DestroysStream) {
  EXPECT_CALL(*client(), Created());

  SendCreatedNotification();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(Mock::VerifyAndClear(client()));

  ExpectHandleWillCallDeleter();

  ResetClientReceiver();
  base::RunLoop().RunUntilIdle();

  VerifyDeleterWasCalled();
}

}  // namespace content
