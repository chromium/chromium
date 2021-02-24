// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_speech_recognition_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

namespace {
const char kTestResult[] = "Pictures of the moon";
}  // namespace

namespace content {

FakeSpeechRecognitionManager::FakeSpeechRecognitionManager()
    : session_id_(0),
      listener_(nullptr),
      fake_result_(kTestResult),
      did_cancel_all_(false),
      should_send_fake_response_(true),
      delegate_(nullptr) {}

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
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  recognition_started_closure_ = runner->QuitClosure();
  runner->Run();
}

void FakeSpeechRecognitionManager::SetFakeResult(const std::string& value) {
  fake_result_ = value;
}

int FakeSpeechRecognitionManager::CreateSession(
    const SpeechRecognitionSessionConfig& config) {
  VLOG(1) << "FAKE CreateSession invoked.";
  EXPECT_EQ(0, session_id_);
  EXPECT_EQ(nullptr, listener_);
  listener_ = config.event_listener.get();
  if (config.grammars.size() > 0)
    grammar_ = config.grammars[0].url.spec();
  session_ctx_ = config.initial_context;
  session_config_ = config;
  session_id_ = 1;
  return session_id_;
}

void FakeSpeechRecognitionManager::StartSession(int session_id) {
  VLOG(1) << "FAKE StartSession invoked.";
  EXPECT_EQ(session_id, session_id_);
  EXPECT_TRUE(listener_ != nullptr);

  if (delegate_)
    delegate_->GetEventListener()->OnRecognitionStart(session_id_);

  if (should_send_fake_response_) {
    // Give the fake result in a short while.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FakeSpeechRecognitionManager::SetFakeRecognitionResult,
            // This class does not need to be refcounted (typically done by
            // PostTask) since it will outlive the test and gets released only
            // when the test shuts down. Disabling refcounting here saves a bit
            // of unnecessary code and the factory method can return a plain
            // pointer below as required by the real code.
            base::Unretained(this)));
  }
  if (!recognition_started_closure_.is_null()) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(recognition_started_closure_));
  }
}

void FakeSpeechRecognitionManager::AbortSession(int session_id) {
  VLOG(1) << "FAKE AbortSession invoked.";
  EXPECT_EQ(session_id_, session_id);
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

void FakeSpeechRecognitionManager::SetFakeRecognitionResult() {
  if (!session_id_)  // Do a check in case we were cancelled..
    return;

  VLOG(1) << "Setting fake recognition result.";
  listener_->OnAudioEnd(session_id_);
  blink::mojom::SpeechRecognitionResultPtr result =
      blink::mojom::SpeechRecognitionResult::New();
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::ASCIIToUTF16(kTestResult), 1.0));
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(std::move(result));
  listener_->OnRecognitionResults(session_id_, results);
  listener_->OnRecognitionEnd(session_id_);
  session_id_ = 0;
  listener_ = nullptr;
  VLOG(1) << "Finished setting fake recognition result.";
}

}  // namespace content
