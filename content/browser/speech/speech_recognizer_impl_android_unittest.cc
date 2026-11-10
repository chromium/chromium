// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl_android.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/test/test_future.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/mock_speech_recognition_event_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::NiceMock;

constexpr int kSessionId = 1;

}  // namespace

class SpeechRecognizerImplAndroidTest : public testing::Test {
 protected:
  void SetUp() override {
    base::test::TestFuture<void> created_future;
    GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<SpeechRecognizerImplAndroid>* out_recognizer,
               SpeechRecognitionEventListener* listener) {
              *out_recognizer =
                  base::MakeRefCounted<SpeechRecognizerImplAndroid>(listener,
                                                                    kSessionId);
            },
            &recognizer_, &listener_),
        created_future.GetCallback());
    ASSERT_TRUE(created_future.Wait());
  }

  void TearDown() override {
    if (recognizer_) {
      base::test::TestFuture<void> reset_future;
      GetIOThreadTaskRunner({})->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              [](scoped_refptr<SpeechRecognizerImplAndroid> recognizer) {
                recognizer.reset();
              },
              std::move(recognizer_)),
          reset_future.GetCallback());
      ASSERT_TRUE(reset_future.Wait());
    }
    // Flush pending UI tasks so UiPeer's DeleteOnUIThread deleter executes.
    base::test::TestFuture<void> ui_flush_future;
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        ui_flush_future.GetCallback());
    ASSERT_TRUE(ui_flush_future.Wait());
  }

  void FlushIOThread() {
    base::test::TestFuture<void> flush_future;
    GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                                flush_future.GetCallback());
    ASSERT_TRUE(flush_future.Wait());
  }

  void SimulateAudioStart() {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::OnAudioStartOnIOThread,
                       base::Unretained(recognizer_.get())));
  }

  void SimulateAudioEnd() {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::OnAudioEndOnIOThread,
                       base::Unretained(recognizer_.get())));
  }

  void SimulateRecognitionEnd() {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognizerImplAndroid::OnRecognitionEndOnIOThread,
                       base::Unretained(recognizer_.get())));
  }

  SpeechRecognizerImplAndroid* recognizer() { return recognizer_.get(); }

  BrowserTaskEnvironment task_environment_{
      BrowserTaskEnvironment::REAL_IO_THREAD};
  NiceMock<MockSpeechRecognitionEventListener> listener_;
  scoped_refptr<SpeechRecognizerImplAndroid> recognizer_;
};

namespace {

TEST_F(SpeechRecognizerImplAndroidTest,
       StopAudioCaptureBeforeStartEndsRecognitionOnce) {
  base::test::TestFuture<void> ended_future;
  auto quit_on_ui =
      base::BindPostTaskToCurrentDefault(ended_future.GetCallback());
  EXPECT_CALL(listener_, OnRecognitionEnd(kSessionId))
      .Times(1)
      .WillOnce([quit_on_ui = std::move(quit_on_ui)](int) mutable {
        std::move(quit_on_ui).Run();
      });

  // Calling StopAudioCapture() before UiPeer::StartRecognition() creates
  // `j_recognition_` must still end the session once.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImplAndroid::StopAudioCapture,
                                base::Unretained(recognizer())));
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognizerImplAndroid::StopAudioCapture,
                                base::Unretained(recognizer())));
  ASSERT_TRUE(ended_future.Wait());

  base::test::TestFuture<bool> active_future;
  GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](SpeechRecognizerImplAndroid* recognizer) {
            recognizer->StopAudioCapture();
            return recognizer->IsActive();
          },
          base::Unretained(recognizer())),
      active_future.GetCallback());
  EXPECT_FALSE(active_future.Get());

  // Ensure any trailing UI->IO hop would have executed before verifying mock.
  FlushIOThread();
}

TEST_F(SpeechRecognizerImplAndroidTest,
       StopAudioCaptureWhileAwaitingFinalResultIgnored) {
  EXPECT_CALL(listener_, OnAudioStart(kSessionId)).Times(1);
  EXPECT_CALL(listener_, OnAudioEnd(kSessionId)).Times(1);
  EXPECT_CALL(listener_, OnRecognitionEnd(kSessionId)).Times(0);

  SimulateAudioStart();
  SimulateAudioEnd();

  base::test::TestFuture<bool> active_future;
  GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](SpeechRecognizerImplAndroid* recognizer) {
            recognizer->StopAudioCapture();
            return recognizer->IsActive() && !recognizer->IsCapturingAudio();
          },
          base::Unretained(recognizer())),
      active_future.GetCallback());
  EXPECT_TRUE(active_future.Get());

  // Flush IO after UI pumped `active_future` so an unguarded StopAudioCapture()
  // UI->IO OnRecognitionEnd task would be caught here.
  FlushIOThread();
  testing::Mock::VerifyAndClearExpectations(&listener_);

  base::test::TestFuture<void> ended_future;
  auto quit_on_ui =
      base::BindPostTaskToCurrentDefault(ended_future.GetCallback());
  EXPECT_CALL(listener_, OnRecognitionEnd(kSessionId))
      .Times(1)
      .WillOnce([quit_on_ui = std::move(quit_on_ui)](int) mutable {
        std::move(quit_on_ui).Run();
      });
  SimulateRecognitionEnd();
  ASSERT_TRUE(ended_future.Wait());
}

}  // namespace

}  // namespace content
