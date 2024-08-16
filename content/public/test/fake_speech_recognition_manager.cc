// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_speech_recognition_manager.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/test/test_utils.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestResult[] = "Pictures of the moon";
}  // namespace

namespace content {

FakeSpeechRecognitionManager::FakeSpeechRecognitionManager()
    : session_id_(0), listener_(nullptr), fake_result_(kTestResult) {}

void FakeSpeechRecognitionManager::SetDelegate(
    SpeechRecognitionManagerDelegate* delegate) {
  delegate_ = delegate;
}

FakeSpeechRecognitionManager::~FakeSpeechRecognitionManager() {
  // Expect the owner of |delegate_| to cleanup our reference before we shut
  // down, just to be safe as we do not own |delegate_|.
  DCHECK(!delegate_);
}

void FakeSpeechRecognitionManager::WaitForRecognitionStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::RunLoop runner;
  recognition_started_closure_ = runner.QuitClosure();
  runner.Run();
}

void FakeSpeechRecognitionManager::WaitForRecognitionEnded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Take no action if recognition is not currently running.
  if (session_id_ == 0)
    return;
  base::RunLoop runner;
  recognition_ended_closure_ = runner.QuitClosure();
  runner.Run();
}

void FakeSpeechRecognitionManager::SendFakeResponse(
    bool end_recognition,
    base::OnceClosure on_fake_response_sent) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The session must be started.
  EXPECT_NE(session_id_, 0);
  EXPECT_TRUE(listener_ != nullptr);
  on_fake_response_sent_closure_ = std::move(on_fake_response_sent);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSpeechRecognitionManager::SetFakeRecognitionResult,
                     base::Unretained(this), end_recognition));
}

void FakeSpeechRecognitionManager::OnRecognitionStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_recognizing_ = true;
  // Complete the closure on the UI thread instead of the IO thread to avoid
  // threading issues.
  if (recognition_started_closure_)
    std::move(recognition_started_closure_).Run();
}

void FakeSpeechRecognitionManager::OnRecognitionEnded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_recognizing_ = false;
  // Complete the closure on the UI thread instead of the IO thread to avoid
  // threading issues.
  if (recognition_ended_closure_)
    std::move(recognition_ended_closure_).Run();
}

void FakeSpeechRecognitionManager::OnFakeResponseSent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (on_fake_response_sent_closure_) {
    std::move(on_fake_response_sent_closure_).Run();
  }
}

void FakeSpeechRecognitionManager::SetFakeResult(const std::string& value,
                                                 bool is_final) {
  fake_result_ = value;
  is_final_ = is_final;
}

int FakeSpeechRecognitionManager::CreateSession(
    const SpeechRecognitionSessionConfig& config) {
  return CreateSession(std::move(config), mojo::NullReceiver(),
                       mojo::NullRemote(), std::nullopt);
}

int FakeSpeechRecognitionManager::CreateSession(
    const SpeechRecognitionSessionConfig& config,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        client_remote,
    std::optional<SpeechRecognitionAudioForwarderConfig>
        audio_forwarder_config) {
  VLOG(1) << "FAKE CreateSession invoked.";
  // FakeSpeechRecognitionManager only allows one active session at a time.
  EXPECT_EQ(0, session_id_);
  EXPECT_EQ(nullptr, listener_.get());
  listener_ = config.event_listener.get();
  if (config.grammars.size() > 0)
    grammar_ = config.grammars[0].url.spec();
  session_ctx_ = config.initial_context;
  session_config_ = config;
  session_id_ = 1;
  has_sent_result_ = false;
  return session_id_;
}

void FakeSpeechRecognitionManager::StartSession(int session_id) {
  VLOG(1) << "FAKE StartSession invoked.";
  EXPECT_EQ(session_id, session_id_);
  EXPECT_TRUE(listener_ != nullptr);

  listener_->OnRecognitionStart(session_id_);
  // Delegate can get a copy of events.
  if (delegate_)
    delegate_->GetEventListener()->OnRecognitionStart(session_id_);

  if (should_send_fake_response_) {
    // Give the fake result in a short while.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FakeSpeechRecognitionManager::SetFakeRecognitionResult,
            // This class does not need to be refcounted (typically done by
            // PostTask) since it will outlive the test and gets released only
            // when the test shuts down. Disabling refcounting here saves a bit
            // of unnecessary code and the factory method can return a plain
            // pointer below as required by the real code.
            base::Unretained(this), true /* end recognition */));
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSpeechRecognitionManager::OnRecognitionStarted,
                     base::Unretained(this)));
}

void FakeSpeechRecognitionManager::AbortSession(int session_id) {
  VLOG(1) << "FAKE AbortSession invoked.";
  EXPECT_EQ(session_id_, session_id);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSpeechRecognitionManager::OnRecognitionEnded,
                     base::Unretained(this)));
  session_id_ = 0;
  listener_ = nullptr;
}

void FakeSpeechRecognitionManager::StopAudioCaptureForSession(int session_id) {
  VLOG(1) << "StopRecording invoked.";
  EXPECT_EQ(session_id_, session_id);
  // Nothing to do here since we aren't really recording.
}

void FakeSpeechRecognitionManager::AbortAllSessionsForRenderFrame(
    int render_process_id,
    int render_frame_id) {
  VLOG(1) << "CancelAllRequestsWithDelegate invoked.";
  EXPECT_TRUE(should_send_fake_response_ ||
              (session_ctx_.render_process_id == render_process_id &&
               session_ctx_.render_frame_id == render_frame_id));
  did_cancel_all_ = true;
}

const SpeechRecognitionSessionConfig&
FakeSpeechRecognitionManager::GetSessionConfig(int session_id) {
  EXPECT_EQ(session_id, session_id_);
  return session_config_;
}

SpeechRecognitionSessionContext FakeSpeechRecognitionManager::GetSessionContext(
    int session_id) {
  EXPECT_EQ(session_id, session_id_);
  return session_ctx_;
}

bool FakeSpeechRecognitionManager::UseOnDeviceSpeechRecognition(
    const SpeechRecognitionSessionConfig& config) {
  return false;
}

void FakeSpeechRecognitionManager::SetFakeRecognitionResult(
    bool end_recognition) {
  if (!session_id_)  // Do a check in case we were cancelled..
    return;
  VLOG(1) << "Setting fake recognition result.";
  if (!has_sent_result_) {
    listener_->OnAudioStart(session_id_);
    listener_->OnSoundStart(session_id_);
    has_sent_result_ = true;
  }
  media::mojom::WebSpeechRecognitionResultPtr result =
      media::mojom::WebSpeechRecognitionResult::New();
  result->hypotheses.push_back(media::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16(fake_result_), 1.0));
  // If `is_provisional` is true, then the result is an interim result that
  // could be changed. Otherwise, it's a final result. Consequently,
  // `is_provisional` is the converse of `is_final`.
  result->is_provisional = !is_final_;
  std::vector<media::mojom::WebSpeechRecognitionResultPtr> results;
  results.push_back(std::move(result));
  listener_->OnRecognitionResults(session_id_, results);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeSpeechRecognitionManager::OnFakeResponseSent,
                     base::Unretained(this)));
  if (end_recognition) {
    // End recognition. Note that in normal SpeechRecognitionManager, a session
    // is not ended after the final result is sent. This behavior is just
    // to make testing easier.
    // Check if the listener has destructed itself after a final result.
    if (listener_) {
      listener_->OnAudioEnd(session_id_);
      listener_->OnRecognitionEnd(session_id_);
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&FakeSpeechRecognitionManager::OnRecognitionEnded,
                         base::Unretained(this)));
    }
    session_id_ = 0;
    listener_ = nullptr;
  }
  VLOG(1) << "Finished setting fake recognition result.";
}

void FakeSpeechRecognitionManager::SendFakeError(
    base::OnceClosure on_fake_error_sent) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EXPECT_NE(session_id_, 0);
  EXPECT_TRUE(listener_ != nullptr);
  on_fake_error_sent_closure_ = std::move(on_fake_error_sent);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeSpeechRecognitionManager::SendFakeSpeechRecognitionError,
          base::Unretained(this)));
}

void FakeSpeechRecognitionManager::SendFakeSpeechRecognitionError() {
  if (!session_id_)
    return;

  VLOG(1) << "Sending fake recognition error.";
  listener_->OnRecognitionError(
      session_id_, *media::mojom::SpeechRecognitionError::New(
                       media::mojom::SpeechRecognitionErrorCode::kNetwork,
                       media::mojom::SpeechAudioErrorDetails::kNone));
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FakeSpeechRecognitionManager::OnFakeErrorSent,
                                base::Unretained(this)));
  VLOG(1) << "Finished sending fake recognition error.";
}

void FakeSpeechRecognitionManager::OnFakeErrorSent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (on_fake_error_sent_closure_) {
    std::move(on_fake_error_sent_closure_).Run();
  }
}

}  // namespace content
