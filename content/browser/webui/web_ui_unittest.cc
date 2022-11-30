// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebUITest : public testing::Test {
 public:
  WebUITest() = default;
  ~WebUITest() override = default;

  void SetUp() override { web_ui_ = std::make_unique<content::TestWebUI>(); }

  std::unique_ptr<content::TestWebUI> web_ui_;
};

namespace {

void HandleTestMessage(int number, bool conditional, const std::string& text) {
  ASSERT_EQ(11, number);
  ASSERT_TRUE(conditional);
  ASSERT_EQ("test text", text);
}

}  // namespace

TEST_F(WebUITest, TestHandler) {
  web_ui_->RegisterHandlerCallback("testMessage",
                                   base::BindRepeating(&HandleTestMessage));
  base::Value::List args;
  args.Append(11);
  args.Append(true);
  args.Append("test text");
  web_ui_->HandleReceivedMessage("testMessage", args);
}
