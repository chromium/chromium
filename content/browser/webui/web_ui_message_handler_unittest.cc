// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_message_handler.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TestWebUIMessageHandler : public WebUIMessageHandler {
 public:
  TestWebUIMessageHandler() { set_web_ui(&test_web_ui_); }

  ~TestWebUIMessageHandler() override {
    // The test handler unusually owns its own TestWebUI, so we make sure to
    // unbind it from the base class before the derived class is destroyed.
    set_web_ui(nullptr);
  }

  void RegisterMessages() override {}

  int on_javascript_allowed_calls() { return on_javascript_allowed_calls_; }
  int on_javascript_disallowed_calls() {
    return on_javascript_disallowed_calls_;
  }

 private:
  TestWebUI test_web_ui_;

  void OnJavascriptAllowed() override { ++on_javascript_allowed_calls_; }
  void OnJavascriptDisallowed() override { ++on_javascript_disallowed_calls_; }

  int on_javascript_allowed_calls_ = 0;
  int on_javascript_disallowed_calls_ = 0;
};

TEST(WebUIMessageHandlerTest, AllowAndDisallowJavascript) {
  TestWebUIMessageHandler handler;

  EXPECT_FALSE(handler.IsJavascriptAllowed());
  EXPECT_EQ(0, handler.on_javascript_allowed_calls());
  EXPECT_EQ(0, handler.on_javascript_disallowed_calls());

  handler.AllowJavascriptForTesting();
  EXPECT_TRUE(handler.IsJavascriptAllowed());
  EXPECT_EQ(1, handler.on_javascript_allowed_calls());
  EXPECT_EQ(0, handler.on_javascript_disallowed_calls());

  // Two calls to AllowJavascript don't trigger OnJavascriptAllowed twice.
  handler.AllowJavascriptForTesting();
  EXPECT_TRUE(handler.IsJavascriptAllowed());
  EXPECT_EQ(1, handler.on_javascript_allowed_calls());
  EXPECT_EQ(0, handler.on_javascript_disallowed_calls());

  handler.DisallowJavascript();
  EXPECT_FALSE(handler.IsJavascriptAllowed());
  EXPECT_EQ(1, handler.on_javascript_allowed_calls());
  EXPECT_EQ(1, handler.on_javascript_disallowed_calls());

  // Two calls to DisallowJavascript don't trigger OnJavascriptDisallowed twice.
  handler.DisallowJavascript();
  EXPECT_FALSE(handler.IsJavascriptAllowed());
  EXPECT_EQ(1, handler.on_javascript_allowed_calls());
  EXPECT_EQ(1, handler.on_javascript_disallowed_calls());
}

}  // namespace content
