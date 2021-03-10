// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_FAKE_WEB_FRAME_INTERNAL_H_
#define IOS_WEB_TEST_FAKES_FAKE_WEB_FRAME_INTERNAL_H_

#include "ios/web/js_messaging/web_frame_internal.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"

namespace web {

class JavaScriptContentWorld;

class FakeWebFrameInternal : public FakeWebFrame, public WebFrameInternal {
 public:
  FakeWebFrameInternal(const std::string& frame_id,
                       bool is_main_frame,
                       GURL security_origin);

  // Returns the JavaScriptContentWorld parameter value received in the last
  // call to |CallJavaScriptFunctionInContentWorld|.
  JavaScriptContentWorld* last_received_content_world();

  // WebFrame:
  // NOTE: These WebFrame overrides simply call the FakeWebFrame implementation.
  std::string GetFrameId() const override;
  bool IsMainFrame() const override;
  GURL GetSecurityOrigin() const override;
  bool CanCallJavaScriptFunction() const override;
  BrowserState* GetBrowserState() override;
  bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters) override;
  bool CallJavaScriptFunction(
      const std::string& name,
      const std::vector<base::Value>& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;

  // WebFrameInternal:
  // If |CanCallJavaScriptFunction()| is true, the JavaScript call which would
  // be executed by a real WebFrame will be added to |java_script_calls_|.
  // Returns the value of |CanCallJavaScriptFunction()|. |content_world| is
  // stored to |last_received_content_world_|.
  bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const std::vector<base::Value>& parameters,
      JavaScriptContentWorld* content_world) override;
  // If |CanCallJavaScriptFunction()| is true, the JavaScript call which would
  // be executed by a real WebFrame will be added to |java_script_calls_|.
  // Returns the value of |CanCallJavaScriptFunction()|.
  // |callback| will be executed with the value passed in to
  // AddJsResultForFunctionCall() or null if no such result has been added.
  // |content_world| is stored to |last_received_content_world_|.
  bool CallJavaScriptFunctionInContentWorld(
      const std::string& name,
      const std::vector<base::Value>& parameters,
      JavaScriptContentWorld* content_world,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;

  ~FakeWebFrameInternal() override;

 private:
  JavaScriptContentWorld* last_received_content_world_;
};

// A fake web frame representing the main frame with a |frame_id_| of
// |kMainFakeFrameId|.
class FakeMainWebFrameInternal : public FakeWebFrameInternal {
 public:
  explicit FakeMainWebFrameInternal(GURL security_origin);
  ~FakeMainWebFrameInternal() override;
};

// A fake web frame representing a child frame with a |frame_id_| of
// |kChildFakeFrameId|.
class FakeChildWebFrameInternal : public FakeWebFrameInternal {
 public:
  explicit FakeChildWebFrameInternal(GURL security_origin);
  ~FakeChildWebFrameInternal() override;
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_FAKE_WEB_FRAME_INTERNAL_H_
