// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_WEB_UI_LISTENER_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_TEST_WEB_UI_LISTENER_OBSERVER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "content/public/test/test_web_ui.h"

namespace content {

// A test observer that allows blocking waits on WebUI calls back to javascript
// listeners using cr.webUIListenerCallback.
class TestWebUIListenerObserver : public TestWebUI::JavascriptCallObserver {
 public:
  // Constructs a TestWebUIListenerObserver which will observer |web_ui| for
  // cr.webUIListenerCallback calls with the first argument |listener_name|.
  TestWebUIListenerObserver(content::TestWebUI* web_ui,
                            const std::string& listener_name);
  ~TestWebUIListenerObserver() override;

  TestWebUIListenerObserver(const TestWebUIListenerObserver& other) = delete;
  TestWebUIListenerObserver& operator=(const TestWebUIListenerObserver& other) =
      delete;

  // Waits for the listener call to happen.
  void Wait();

  // Only callable after Wait() has returned. Contains the arguments passed to
  // the listener.
  base::Value::List& args() { return call_args_.value(); }

 private:
  void OnJavascriptFunctionCalled(
      const TestWebUI::CallData& call_data) override;

  raw_ptr<content::TestWebUI> web_ui_;
  std::string listener_name_;
  base::RunLoop run_loop_;

  // Only filled when a matching listener call has been observed.
  std::optional<base::Value::List> call_args_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_WEB_UI_LISTENER_OBSERVER_H_
