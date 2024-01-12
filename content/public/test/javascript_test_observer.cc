// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/javascript_test_observer.h"

#include "base/run_loop.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

namespace content {

TestMessageHandler::TestMessageHandler() : ok_(true) {
}

TestMessageHandler::~TestMessageHandler() {
}

void TestMessageHandler::SetError(const std::string& message) {
  ok_ = false;
  error_message_ = message;
}

void TestMessageHandler::Reset() {
  ok_ = true;
  error_message_.clear();
}

JavascriptTestObserver::JavascriptTestObserver(WebContents* web_contents,
                                               TestMessageHandler* handler)
    : WebContentsObserver(web_contents),
      handler_(handler),
      running_(false),
      finished_(false) {
  Reset();
}

JavascriptTestObserver::~JavascriptTestObserver() {
}

bool JavascriptTestObserver::Run() {
  // Messages may have arrived before Run was called.
  if (!finished_) {
    CHECK(!running_);
    running_ = true;
    loop_.Run();
    running_ = false;
  }
  return handler_->ok();
}

void JavascriptTestObserver::Reset() {
  CHECK(!running_);
  running_ = false;
  finished_ = false;
  handler_->Reset();
}

void JavascriptTestObserver::DomOperationResponse(
    RenderFrameHost* render_frame_host,
    const std::string& json_string) {
  // We might receive responses for other script execution, but we only
  // care about the test finished message.
  TestMessageHandler::MessageResponse response =
      handler_->HandleMessage(json_string);

  if (response == TestMessageHandler::DONE) {
    EndTest();
  } else {
    Continue();
  }
}

void JavascriptTestObserver::Continue() {
}

void JavascriptTestObserver::EndTest() {
  finished_ = true;
  if (running_) {
    running_ = false;
    loop_.QuitWhenIdle();
  }
}

}  // namespace content
