// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/shell_runner.h"

#include "gin/converter.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"

namespace gin {

typedef V8Test RunnerTest;

TEST_F(RunnerTest, Run) {
  std::string source = "this.result = 'PASS';\n";

  ShellRunnerDelegate delegate;
  v8::Isolate* isolate = instance_->isolate();
  ShellRunner runner(&delegate, isolate);
  Runner::Scope scope(&runner);
  runner.Run(source, "test_data.js");

  std::string result;
  EXPECT_TRUE(Converter<std::string>::FromV8(
      isolate,
      runner.global()
          ->Get(isolate->GetCurrentContext(), StringToV8(isolate, "result"))
          .ToLocalChecked(),
      &result));
  EXPECT_EQ("PASS", result);
}

}  // namespace gin
