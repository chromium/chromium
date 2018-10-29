// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_

#include "ios/web/public/web_state/web_frame.h"

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
  // This method will not call JavaScript and immediately return false.
  // |callback| will not be called.
  bool CallJavaScriptFunction(
      std::string name,
      const std::vector<base::Value>& parameters,
      base::OnceCallback<void(const base::Value*)> callback,
      base::TimeDelta timeout) override;
  std::string last_javascript_call() { return last_javascript_call_; }

 private:
  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  std::string frame_id_;
  // Whether or not the receiver represents the main frame.
  bool is_main_frame_ = false;
  // The security origin associated with this frame.
  GURL security_origin_;
  // The last Javascript script that was called, converted as a string.
  std::string last_javascript_call_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_
