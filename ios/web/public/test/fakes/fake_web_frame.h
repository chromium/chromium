// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_

#include <map>
#include <memory>
#include <vector>

#include "ios/web/public/js_messaging/web_frame.h"

namespace web {

// Frame id constants which can be used to initialize FakeWebFrame.
// Frame ids are base16 string of 128 bit numbers.
extern const char kMainFakeFrameId[];
extern const char kInvalidFrameId[];
extern const char kChildFakeFrameId[];
extern const char kChildFakeFrameId2[];

// A fake web frame to use for testing.
class FakeWebFrame : public WebFrame {
 public:
  // Creates a web frame. |frame_id| must be a string representing a valid
  // hexadecimal number.
  FakeWebFrame(const std::string& frame_id,
               bool is_main_frame,
               GURL security_origin);
  ~FakeWebFrame() override;

  std::string GetFrameId() const override;
  bool IsMainFrame() const override;
  GURL GetSecurityOrigin() const override;
  bool CanCallJavaScriptFunction() const override;
  BrowserState* GetBrowserState() override;
  // If |can_call_function_| is true, the JavaScript call which would be
  // executed by a real WebFrame will be added to |java_script_calls_|. Returns
  // the value of |can_call_function_|.
  bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters) override;
  // If |can_call_function_| is true, the JavaScript call which would be
  // executed by a real WebFrame will be added to |java_script_calls_|. Returns
  // the value of |can_call_function_|.
  // |callback| will be executed with the value passed in to
  // AddJsResultForFunctionCall() or null if no such result has been added.
  bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;

  // Returns the most recent JavaScript handler call made to this frame.
  std::string GetLastJavaScriptCall() const {
    return java_script_calls_.size() == 0 ? "" : java_script_calls_.back();
  }

  // Returns |javascript_calls|. Use LastJavaScriptCall() if possible.
  const std::vector<std::string>& GetJavaScriptCallHistory() {
    return java_script_calls_;
  }

  // Sets the browser state associated with this frame.
  void set_browser_state(BrowserState* browser_state) {
    browser_state_ = browser_state;
  }

  // Sets |js_result| that will be passed into callback for |name| function
  // call. The same result will be pass regardless of call arguments.
  void AddJsResultForFunctionCall(std::unique_ptr<base::Value> js_result,
                                  const std::string& function_name);

  void set_force_timeout(bool force_timeout) { force_timeout_ = force_timeout; }

  // Sets return value |can_call_function_| of CanCallJavaScriptFunction(),
  // which defaults to true.
  void set_can_call_function(bool can_call_function) {
    can_call_function_ = can_call_function;
  }

 private:
  // Map holding values to be passed in CallJavaScriptFunction() callback. Keyed
  // by JavaScript function |name| expected to be passed into
  // CallJavaScriptFunction().
  std::map<std::string, std::unique_ptr<base::Value>> result_map_;
  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  std::string frame_id_;
  // Whether or not the receiver represents the main frame.
  bool is_main_frame_ = false;
  // The security origin associated with this frame.
  GURL security_origin_;
  // Vector holding history of all javascript handler calls made in this frame.
  // The calls are sorted with the most recent appended at the end.
  std::vector<std::string> java_script_calls_;
  // The return value of CanCallJavaScriptFunction().
  bool can_call_function_ = true;
  // When set to true, will force calls to CallJavaScriptFunction to fail with
  // timeout.
  bool force_timeout_ = false;
  BrowserState* browser_state_;
};

// A fake web frame representing the main frame with a |frame_id_| of
// |kMainFakeFrameId|.
class FakeMainWebFrame : public FakeWebFrame {
 public:
  explicit FakeMainWebFrame(GURL security_origin);
  ~FakeMainWebFrame() override;
};

// A fake web frame representing a child frame with a |frame_id_| of
// |kChildFakeFrameId|.
class FakeChildWebFrame : public FakeWebFrame {
 public:
  explicit FakeChildWebFrame(GURL security_origin);
  ~FakeChildWebFrame() override;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_
