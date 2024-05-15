// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_FAKE_WEB_FRAME_IMPL_H_
#define IOS_WEB_TEST_FAKES_FAKE_WEB_FRAME_IMPL_H_

#include <map>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "base/values.h"
#include "ios/web/js_messaging/web_frame_internal.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"

namespace web {

class JavaScriptContentWorld;

class FakeWebFrameImpl : public FakeWebFrame, public WebFrameInternal {
 public:
  FakeWebFrameImpl(const std::string& frame_id,
                   bool is_main_frame,
                   GURL security_origin);

  // WebFrame:
  WebFrameInternal* GetWebFrameInternal() override;
  std::string GetFrameId() const override;
  bool IsMainFrame() const override;
  GURL GetSecurityOrigin() const override;
  BrowserState* GetBrowserState() override;
  bool CallJavaScriptFunction(const std::string& name,
                              const base::Value::List& parameters) override;
  bool CallJavaScriptFunction(
      const std::string& name,
      const base::Value::List& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;
  bool ExecuteJavaScript(const std::u16string& script) override;
  bool ExecuteJavaScript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value*)> callback) override;
  bool ExecuteJavaScript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value*, NSError*)> callback) override;
  base::WeakPtr<WebFrame> AsWeakPtr() override;

  // FakeWebFrame:
  std::u16string GetLastJavaScriptCall() const override;
  const std::vector<std::u16string>& GetJavaScriptCallHistory() override;
  void ClearJavaScriptCallHistory() override;
  void set_browser_state(BrowserState* browser_state) override;
  void AddJsResultForFunctionCall(base::Value* js_result,
                                  const std::string& function_name) override;
  void AddResultForExecutedJs(base::Value* js_result,
                              const std::u16string& executed_js) override;
  void set_force_timeout(bool force_timeout) override;
  void set_call_java_script_function_callback(
      base::RepeatingClosure callback) override;

  // WebFrameInternal:
  // The JavaScript call which would be executed by a real WebFrame will be
  // added to `java_script_calls_`. Always returns true.
  bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const base::Value::List& parameters,
      JavaScriptContentWorld* content_world) override;
  // The JavaScript call which would be executed by a real WebFrame will be
  // added to `java_script_calls_`. Always returns true.
  // `callback` will be executed with the value passed in to
  // AddJsResultForFunctionCall() or null if no such result has been added.
  bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const base::Value::List& parameters,
      JavaScriptContentWorld* content_world,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;
  // The JavaScript call which would be executed by a real WebFrame will be
  // added to `java_script_calls_`.
  // `callback` will be executed with the value passed in to
  // AddResultForExecutedJs() or null if no such result has been added.
  bool ExecuteJavaScriptInContentWorld(
      const std::u16string& script,
      JavaScriptContentWorld* content_world,
      ExecuteJavaScriptCallbackWithError callback) override;

  ~FakeWebFrameImpl() override;

 private:
  // Map holding values to be passed in CallJavaScriptFunction() callback. Keyed
  // by JavaScript function `name` expected to be passed into
  // CallJavaScriptFunction().
  std::map<std::string, base::Value*> result_map_;
  // Map holding values to be passed in ExecuteJavaScript() callback. Keyed by
  // by JavaScript expected to be passed to ExecuteJavaScript().
  std::map<std::u16string, base::Value*> executed_js_result_map_;
  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  std::string frame_id_;
  // Whether or not the receiver represents the main frame.
  bool is_main_frame_ = false;
  // The security origin associated with this frame.
  GURL security_origin_;
  // Vector holding history of all javascript handler calls made in this frame.
  // The calls are sorted with the most recent appended at the end.
  std::vector<std::u16string> java_script_calls_;
  // When set to true, will force calls to CallJavaScriptFunction to fail with
  // timeout.
  bool force_timeout_ = false;
  raw_ptr<BrowserState> browser_state_;

  base::RepeatingClosure call_java_script_function_callback_;

  base::WeakPtrFactory<WebFrame> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_FAKE_WEB_FRAME_IMPL_H_
