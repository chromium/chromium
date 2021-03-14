// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_message_handler.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(WebUIMessageHandlerTest, ExtractIntegerValue) {
  base::ListValue list;
  int value;
  constexpr int zero_value = 0;
  constexpr int neg_value = -1234;
  constexpr int pos_value = 1234;
  static const char zero_string[] = "0";
  static const char neg_string[] = "-1234";
  static const char pos_string[] = "1234";

  list.AppendInteger(zero_value);
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, zero_value);
  list.Clear();

  list.AppendInteger(neg_value);
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, neg_value);
  list.Clear();

  list.AppendInteger(pos_value);
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, pos_value);
  list.Clear();

  list.AppendString(zero_string);
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, zero_value);
  list.Clear();

  list.AppendString(neg_string);
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, neg_value);
  list.Clear();

  list.AppendString(pos_string);
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, pos_value);
}

TEST(WebUIMessageHandlerTest, ExtractDoubleValue) {
  base::ListValue list;
  double value;
  constexpr double zero_value = 0.0;
  constexpr double neg_value = -1234.5;
  constexpr double pos_value = 1234.5;
  static const char zero_string[] = "0";
  static const char neg_string[] = "-1234.5";
  static const char pos_string[] = "1234.5";

  list.AppendDouble(zero_value);
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, zero_value);
  list.Clear();

  list.AppendDouble(neg_value);
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, neg_value);
  list.Clear();

  list.AppendDouble(pos_value);
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, pos_value);
  list.Clear();

  list.AppendString(zero_string);
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, zero_value);
  list.Clear();

  list.AppendString(neg_string);
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, neg_value);
  list.Clear();

  list.AppendString(pos_string);
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, pos_value);
}

TEST(WebUIMessageHandlerTest, ExtractStringValue) {
  base::ListValue list;
  static const char in_string[] =
      "The facts, though interesting, are irrelevant.";
  list.AppendString(in_string);
  std::u16string out_string = WebUIMessageHandler::ExtractStringValue(&list);
  EXPECT_EQ(base::ASCIIToUTF16(in_string), out_string);
}

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
