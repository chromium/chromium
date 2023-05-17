// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_web_ui_listener_observer.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/public/test/test_web_ui.h"

namespace content {

TestWebUIListenerObserver::TestWebUIListenerObserver(
    content::TestWebUI* web_ui,
    const std::string& listener_name)
    : web_ui_(web_ui), listener_name_(listener_name) {
  web_ui_->AddJavascriptCallObserver(this);
}

TestWebUIListenerObserver::~TestWebUIListenerObserver() {
  web_ui_->RemoveJavascriptCallObserver(this);
}

void TestWebUIListenerObserver::Wait() {
  run_loop_.Run();
}

void TestWebUIListenerObserver::OnJavascriptFunctionCalled(
    const TestWebUI::CallData& call_data) {
  // Ignore subsequent calls after a call matched the listener that this
  // observer is waiting for.
  if (call_args_.has_value())
    return;

  // See WebUIMessageHandler::FireWebUIListener.
  if (call_data.function_name() != "cr.webUIListenerCallback")
    return;
  if (!call_data.arg1() || !call_data.arg1()->is_string() ||
      call_data.arg1()->GetString() != listener_name_) {
    return;
  }

  call_args_.emplace();
  // TestWebUI::CallData supports up to 4 arguments, each of them has a
  // different accessor. arg1() is the listener name for WebUI listeners (see
  // above).
  if (call_data.arg2()) {
    call_args_->Append(call_data.arg2()->Clone());
  }
  if (call_data.arg3()) {
    call_args_->Append(call_data.arg3()->Clone());
  }
  if (call_data.arg4()) {
    call_args_->Append(call_data.arg4()->Clone());
  }
  run_loop_.Quit();
}

}  // namespace content
