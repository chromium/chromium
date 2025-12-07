// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_stream_wrapper.h"

#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/win/mf_mocks.h"
#include "media/base/win/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

static constexpr int kFakeBufferSize = 16;

namespace {

class TestToken : public Microsoft::WRL::RuntimeClass<
                      Microsoft::WRL::RuntimeClassFlags<
                          Microsoft::WRL::RuntimeClassType::ClassicCom>,
                      IUnknown> {
 public:
  TestToken() = default;
  ~TestToken() override = default;
  HRESULT RuntimeClassInitialize() { return S_OK; }
};

ACTION_P(ReturnBuffer, buffer, task_runner_) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(
                                        [](auto arg0, auto buffer) {
                                          std::move(arg0).Run(
                                              buffer.get()
                                                  ? DemuxerStream::kOk
                                                  : DemuxerStream::kAborted,
                                              {buffer});
                                        },
                                        std::move(arg0), buffer));
}

}  // namespace

class MediaFoundationStreamWrapperTest : public testing::Test {
 public:
  MediaFoundationStreamWrapperTest() {
    mf_media_source_ = MakeComPtr<NiceMock<MockMFMediaSource>>();
    video_stream_ =
        CreateMockDemuxerStream(DemuxerStream::VIDEO, /*encrypted=*/false);
    stream_wrapper_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
    video_buffer_ = base::MakeRefCounted<DecoderBuffer>(kFakeBufferSize);
    MediaFoundationStreamWrapper::Create(
        0, mf_media_source_.Get(), video_stream_.get(),
        std::make_unique<NullMediaLog>(), stream_wrapper_task_runner_,
        &mf_video_stream_wrapper_);
  }

  ~MediaFoundationStreamWrapperTest() override {
    mf_video_stream_wrapper_.Reset();
  }

  // Polls the MediaFoundationStreamWrapper object for queued media events.
  // Attempts until specified timeout.
  bool GetNextStreamEvent(base::TimeDelta timeout, IMFMediaEvent** ppEvent) {
    const int maxAttempts = timeout / base::Milliseconds(10);
    int attempts = 0;
    bool eventReceived = false;

    while (maxAttempts > attempts) {
      attempts++;
      if (mf_video_stream_wrapper_->GetEvent(MF_EVENT_FLAG_NO_WAIT, ppEvent) !=
          MF_E_NO_EVENTS_AVAILABLE) {
        eventReceived = true;
        break;
      }
      Sleep(10);
    }
    return eventReceived;
  }

  void RequestSample(ComPtr<IUnknown> spToken) {
    base::WaitableEvent waitRequestSample;

    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(
            [](ComPtr<MediaFoundationStreamWrapper> mf_stream_wrapper,
               ComPtr<IUnknown> token, base::WaitableEvent* done) {
              mf_stream_wrapper->RequestSample(token.Get());
              done->Signal();
            },
            mf_video_stream_wrapper_, spToken, &waitRequestSample));

    waitRequestSample.Wait();
  }

  void VerifyExpectedSampleEvent(ComPtr<IMFMediaEvent> spMediaEvent,
                                 ComPtr<IUnknown> spExpectedToken) {
    MediaEventType met;
    PROPVARIANT pv;
    ComPtr<IUnknown> spSampleUnk;
    ComPtr<IMFSample> spSample;
    ComPtr<IUnknown> spReceivedToken;

    ASSERT_NE(spMediaEvent, nullptr);

    spMediaEvent->GetType(&met);
    spMediaEvent->GetValue(&pv);
    EXPECT_EQ(met, MEMediaSample);
    EXPECT_EQ(pv.vt, VT_UNKNOWN);
    spSampleUnk = pv.punkVal;
    EXPECT_EQ(spSampleUnk->QueryInterface(IID_PPV_ARGS(&spSample)), S_OK);
    EXPECT_EQ(spSample->GetUnknown(MFSampleExtension_Token, IID_IUnknown,
                                   (void**)&spReceivedToken),
              S_OK);
    EXPECT_EQ(spReceivedToken, spExpectedToken);
  }

  void VerifyBufferedPostFlushSamplesProcessed(bool startEvent) {
    ComPtr<IMFMediaEvent> spMediaEvent;
    ComPtr<IUnknown> spToken1;
    ComPtr<IUnknown> spToken2;
    MediaEventType met;

    MakeAndInitialize<TestToken>(&spToken1);
    MakeAndInitialize<TestToken>(&spToken2);

    // Select and flush the stream
    stream_wrapper_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](ComPtr<MediaFoundationStreamWrapper> mf_stream_wrapper) {
              PROPVARIANT pv;
              PropVariantInit(&pv);
              pv.vt = VT_I8;
              pv.hVal.QuadPart = 0;

              mf_stream_wrapper->SetSelected(true);
              mf_stream_wrapper->Flush();
            },
            mf_video_stream_wrapper_));

    // Setup stream to receive data to fulfill only 2 sample requests
    EXPECT_CALL(*video_stream_, OnRead(_))
        .Times(3)
        .WillOnce(ReturnBuffer(video_buffer_, stream_wrapper_task_runner_))
        .WillOnce(ReturnBuffer(video_buffer_, stream_wrapper_task_runner_))
        .WillRepeatedly([] { /*Do Nothing on the 3rd call. While flushed, we
                                would buffer all the data we can.*/
        });

    // Request a sample.  Post-flush Pre-start, this shouldn't be fulfilled.
    RequestSample(spToken1);

    // Verify no samples events are sent
    spMediaEvent.Reset();
    EXPECT_FALSE(
        GetNextStreamEvent(/*timeout*/ base::Milliseconds(200), &spMediaEvent));

    // Start the stream via seek/start event
    stream_wrapper_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](ComPtr<MediaFoundationStreamWrapper> mf_stream_wrapper,
               bool startEvent) {
              PROPVARIANT pv;
              PropVariantInit(&pv);
              pv.vt = VT_I8;
              pv.hVal.QuadPart = 0;

              if (startEvent) {
                mf_stream_wrapper->QueueStartedEvent(&pv);
              } else {
                mf_stream_wrapper->QueueSeekedEvent(&pv);
              }
            },
            mf_video_stream_wrapper_, startEvent));

    // Verify we receive MEStreamStarted Event
    spMediaEvent.Reset();
    EXPECT_TRUE(
        GetNextStreamEvent(/*timeout*/ base::Milliseconds(200), &spMediaEvent));
    spMediaEvent->GetType(&met);
    EXPECT_EQ(met, startEvent ? MEStreamStarted : MEStreamSeeked);

    // Request another 2 samples.  We should fulfill both sample requests with
    // buffered data.
    RequestSample(spToken2);
    RequestSample(nullptr);  // token doesn't matter here. We don't expect a
                             // sample to be available to fulfill this request.

    // Verify we fulfill the first sample request with the buffered data.
    // We know this sample is from buffered data because OnRead was set to only
    // return buffered data once in our mock.
    spMediaEvent.Reset();
    EXPECT_TRUE(
        GetNextStreamEvent(/*timeout*/ base::Milliseconds(200), &spMediaEvent));
    VerifyExpectedSampleEvent(spMediaEvent, spToken1);

    // Verify we fulfill the second sample request with the buffered data
    spMediaEvent.Reset();
    EXPECT_TRUE(
        GetNextStreamEvent(/*timeout*/ base::Milliseconds(200), &spMediaEvent));
    VerifyExpectedSampleEvent(spMediaEvent, spToken2);

    // Verify no samples events are remaining
    spMediaEvent.Reset();
    EXPECT_FALSE(
        GetNextStreamEvent(/*timeout*/ base::Milliseconds(50), &spMediaEvent));
  }

 protected:
  ComPtr<MediaFoundationStreamWrapper> mf_video_stream_wrapper_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> stream_wrapper_task_runner_;
  ComPtr<NiceMock<MockMFMediaSource>> mf_media_source_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  scoped_refptr<DecoderBuffer> video_buffer_;
};

TEST_F(MediaFoundationStreamWrapperTest, VerifySampleProcessingPostStart) {
  ComPtr<IMFMediaEvent> spMediaEvent;
  ComPtr<IUnknown> spToken;
  MediaEventType met;

  MakeAndInitialize<TestToken>(&spToken);

  // Select, flush, and start the stream
  stream_wrapper_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ComPtr<MediaFoundationStreamWrapper> mf_stream_wrapper) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            pv.vt = VT_I8;
            pv.hVal.QuadPart = 0;
            mf_stream_wrapper->SetSelected(true);
            mf_stream_wrapper->Flush();
            mf_stream_wrapper->QueueStartedEvent(&pv);
          },
          mf_video_stream_wrapper_));

  // Verify we receive MEStreamStarted Event
  spMediaEvent.Reset();
  EXPECT_TRUE(
      GetNextStreamEvent(/*timeout*/ base::Milliseconds(200), &spMediaEvent));
  spMediaEvent->GetType(&met);
  EXPECT_EQ(met, MEStreamStarted);

  // Request a sample
  EXPECT_CALL(*video_stream_, OnRead(_))
      .Times(1)
      .WillOnce(ReturnBuffer(video_buffer_,
                             /*task_environment_.GetMainThreadTaskRunner()*/
                             stream_wrapper_task_runner_));
  RequestSample(spToken);

  // Verify sample event is queued and corresponds to the expected token
  spMediaEvent.Reset();
  EXPECT_TRUE(
      GetNextStreamEvent(/*timeout*/ base::Milliseconds(200), &spMediaEvent));
  VerifyExpectedSampleEvent(spMediaEvent, spToken);

  // Verify no more samples are received
  spMediaEvent.Reset();
  EXPECT_FALSE(
      GetNextStreamEvent(/*timeout*/ base::Milliseconds(50), &spMediaEvent));
}

TEST_F(MediaFoundationStreamWrapperTest,
       VerifyNoSampleProcessingPostFlushPreStart) {
  ComPtr<IMFMediaEvent> spMediaEvent;
  ComPtr<IUnknown> spToken;
  MakeAndInitialize<TestToken>(&spToken);

  // Select and flush the stream, but don't start
  stream_wrapper_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ComPtr<MediaFoundationStreamWrapper> mf_stream_wrapper) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            pv.vt = VT_I8;
            pv.hVal.QuadPart = 0;

            mf_stream_wrapper->SetSelected(true);
            mf_stream_wrapper->Flush();
          },
          mf_video_stream_wrapper_));

  // Request a sample
  EXPECT_CALL(*video_stream_, OnRead(_))
      .Times(2)
      .WillOnce(ReturnBuffer(video_buffer_, /**/ stream_wrapper_task_runner_))
      .WillRepeatedly([] { /*Do Nothing on the 2nd call. While flushed, we would
                              buffer all the data we can.*/
      });
  RequestSample(spToken);

  // Verify no samples events are sent post-flush pre-start
  spMediaEvent.Reset();
  EXPECT_FALSE(
      GetNextStreamEvent(/*timeout*/ base::Milliseconds(50), &spMediaEvent));
}

TEST_F(MediaFoundationStreamWrapperTest,
       VerifySamplesBufferedPostFlushProcessedPostStart) {
  VerifyBufferedPostFlushSamplesProcessed(/*startEvent*/ true);
}

TEST_F(MediaFoundationStreamWrapperTest,
       VerifySamplesBufferedPostFlushProcessedPostSeek) {
  VerifyBufferedPostFlushSamplesProcessed(/*startEvent*/ false);
}

}  // namespace media
