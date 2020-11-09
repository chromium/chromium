// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/audio/audio_playback_sink_ios.h"

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "remoting/client/audio/audio_stream_format.h"
#include "remoting/client/audio/fake_async_audio_data_supplier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr AudioStreamFormat kStreamFormat = {2, 2, 44100};
constexpr base::TimeDelta kBufferPlaybackTimeout =
    base::TimeDelta::FromMilliseconds(500);

}  // namespace

class AudioPlaybackSinkIosTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  void FeedStreamFormat();
  void BlockAndRunOnAudioThread(base::OnceClosure closure);
  void Sleep();

  base::Thread audio_thread_{"Chromoting Audio"};
  std::unique_ptr<FakeAsyncAudioDataSupplier> supplier_;
  std::unique_ptr<AudioPlaybackSinkIos> sink_;
};

// Test fixture definitions

void AudioPlaybackSinkIosTest::SetUp() {
  audio_thread_.StartAndWaitForTesting();
  supplier_ = std::make_unique<FakeAsyncAudioDataSupplier>();
  sink_ = std::make_unique<AudioPlaybackSinkIos>();
  sink_->SetDataSupplier(supplier_.get());
}

void AudioPlaybackSinkIosTest::TearDown() {
  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    sink_.reset();
    supplier_.reset();
  }));
  audio_thread_.Stop();
}

void AudioPlaybackSinkIosTest::FeedStreamFormat() {
  sink_->ResetStreamFormat(kStreamFormat);
}

void AudioPlaybackSinkIosTest::BlockAndRunOnAudioThread(
    base::OnceClosure closure) {
  audio_thread_.task_runner()->PostTask(FROM_HERE, std::move(closure));
  audio_thread_.FlushForTesting();
}

void AudioPlaybackSinkIosTest::Sleep() {
  base::PlatformThread::Sleep(kBufferPlaybackTimeout);
}

// Test cases

TEST_F(AudioPlaybackSinkIosTest, Init) {
  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    ASSERT_EQ(0u, supplier_->pending_requests_count());
    FeedStreamFormat();

    // New requests have been enqueued immediately.
    ASSERT_GT(supplier_->pending_requests_count(), 0u);
  }));
}

TEST_F(AudioPlaybackSinkIosTest, NoLingeringRequestsAfterDestruction) {
  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    FeedStreamFormat();
    ASSERT_GT(supplier_->pending_requests_count(), 0u);

    // Delete the audio sink.
    sink_.reset();

    // No lingering pending requests.
    ASSERT_EQ(0u, supplier_->pending_requests_count());
  }));
}

TEST_F(AudioPlaybackSinkIosTest, DestroyWhenPlaying) {
  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    FeedStreamFormat();
    ASSERT_GT(supplier_->pending_requests_count(), 0u);
    supplier_->FulfillAllRequests();

    // Delete the audio sink.
    sink_.reset();

    // No lingering pending requests.
    ASSERT_EQ(0u, supplier_->pending_requests_count());
  }));
}

TEST_F(AudioPlaybackSinkIosTest, BufferUnderrunScenario) {
  size_t max_number_of_requests;
  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    ASSERT_EQ(0u, supplier_->pending_requests_count());
    FeedStreamFormat();
    max_number_of_requests = supplier_->pending_requests_count();
    ASSERT_GT(max_number_of_requests, 0u);
    supplier_->FulfillAllRequests();
    // Old buffers are returned. New request has not come yet.
    ASSERT_EQ(0u, supplier_->pending_requests_count());
  }));

  // Wait for the sink to consume all buffers and return them to the supplier.
  Sleep();

  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    // Audio buffers should now be returned to the supplier. The AudioQueue
    // should be stopped because of buffer underrun.
    ASSERT_EQ(max_number_of_requests, supplier_->pending_requests_count());

    supplier_->FulfillAllRequests();

    ASSERT_EQ(0u, supplier_->pending_requests_count());
  }));

  // Wait for the sink to consume all buffers.
  Sleep();

  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    // Audio buffers should now be returned to the supplier. Buffer underrun
    // again.
    ASSERT_EQ(max_number_of_requests, supplier_->pending_requests_count());
  }));
}

TEST_F(AudioPlaybackSinkIosTest, KeepFulfillingRequestsOneByOne) {
  size_t max_number_of_requests;
  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    supplier_->set_fulfill_requests_immediately(true);
    FeedStreamFormat();
    max_number_of_requests = supplier_->pending_requests_count();
  }));

  size_t number_of_fulfilled_requests = 0;

  // Keep the queue running and verify that the number of fulfilled requests
  // keeps increasing.
  for (int i = 0; i < 5; i++) {
    Sleep();

    BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
      // Make sure the number of pending requests does not exceed
      // |max_number_of_requests|.
      ASSERT_LE(supplier_->pending_requests_count(), max_number_of_requests);

      size_t new_number_of_fulfilled_requests =
          supplier_->fulfilled_requests_count();
      ASSERT_GT(new_number_of_fulfilled_requests, number_of_fulfilled_requests);
      number_of_fulfilled_requests = new_number_of_fulfilled_requests;
    }));
  }
}

TEST_F(AudioPlaybackSinkIosTest, ChangeStreamFormat_NoPendingRequests) {
  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    ASSERT_EQ(0u, supplier_->pending_requests_count());
    FeedStreamFormat();
    size_t max_number_of_requests = supplier_->pending_requests_count();
    ASSERT_GT(max_number_of_requests, 0u);
    supplier_->FulfillAllRequests();
    // Old buffers are returned. New request has not come yet.
    ASSERT_EQ(0u, supplier_->pending_requests_count());
    // Change the sample rate to 48000 now.
    AudioStreamFormat new_stream_format = {2, 2, 48000};
    sink_->ResetStreamFormat(new_stream_format);

    // New pending requests are enqueued.
    ASSERT_EQ(max_number_of_requests, supplier_->pending_requests_count());
  }));
}

TEST_F(AudioPlaybackSinkIosTest, ChangeStreamFormat_WithPendingRequests) {
  size_t max_number_of_requests;
  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    ASSERT_EQ(0u, supplier_->pending_requests_count());
    FeedStreamFormat();
    max_number_of_requests = supplier_->pending_requests_count();
    ASSERT_GT(max_number_of_requests, 0u);
    supplier_->FulfillAllRequests();
    // Old buffers are returned. New request has not come yet.
    ASSERT_EQ(0u, supplier_->pending_requests_count());
  }));

  // Sleep until new requests are enqueued.
  Sleep();

  BlockAndRunOnAudioThread(base::BindLambdaForTesting([&]() {
    // Verify that new requests are enqueued.
    ASSERT_EQ(max_number_of_requests, supplier_->pending_requests_count());

    // Change the sample rate to 48000 now.
    AudioStreamFormat new_stream_format = {2, 2, 48000};
    sink_->ResetStreamFormat(new_stream_format);

    // Same number of enqueued requests.
    ASSERT_EQ(max_number_of_requests, supplier_->pending_requests_count());
  }));
}

}  // namespace remoting
