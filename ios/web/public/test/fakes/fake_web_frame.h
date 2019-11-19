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

class FakeWebFrame : public WebFrame {
 public:
  FakeWebFrame(const std::string& frame_id,
               bool is_main_frame,
               GURL security_origin);
  ~FakeWebFrame() override;

  std::string GetFrameId() const override;
  bool IsMainFrame() const override;
  GURL GetSecurityOrigin() const override;
  bool CanCallJavaScriptFunction() const override;
  // This method will not call JavaScript and immediately return false.
  bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters) override;
  // This method will not call JavaScript and will return the value of
  // |can_call_function_|. Will execute callback with value passed in to
  // AddJsResultForFunctionCall(). If no such value exists, will pass null.
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

  // Sets |js_result| that will be passed into callback for |name| function
  // call. The same result will be pass regardless of call arguments.
  void AddJsResultForFunctionCall(std::unique_ptr<base::Value> js_result,
                                  const std::string& function_name);

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
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_
