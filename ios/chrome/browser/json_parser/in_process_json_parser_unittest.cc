// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/json_parser/in_process_json_parser.h"

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(InProcessJsonParserTest, TestSuccess) {
  base::test::TaskEnvironment environment;

  base::RunLoop run_loop;
  InProcessJsonParser::Parse(
      R"json({"key": 1})json",
      base::BindOnce(
          [](base::Closure quit_closure, base::Value value) {
            ASSERT_TRUE(value.is_dict());
            ASSERT_TRUE(value.FindIntKey("key"));
            EXPECT_EQ(1, *value.FindIntKey("key"));
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()),
      base::BindOnce(
          [](base::Closure quit_closure, const std::string& error) {
            EXPECT_FALSE(true) << "unexpected json parse error: " << error;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST(InProcessJsonParserTest, TestFailure) {
  base::test::TaskEnvironment environment;

  base::RunLoop run_loop;
  InProcessJsonParser::Parse(
      R"json(invalid)json",
      base::BindOnce(
          [](base::Closure quit_closure, base::Value value) {
            EXPECT_FALSE(true) << "unexpected json parse success: " << value;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()),
      base::BindOnce(
          [](base::Closure quit_closure, const std::string& error) {
            EXPECT_TRUE(!error.empty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}
