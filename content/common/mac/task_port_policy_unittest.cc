// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/task_port_policy.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(TaskPortPolicyTest, ParseBootArgs) {
  const struct {
    const char* input;
    const char* output;
  } kCases[] = {
      {"amfi_get_out_of_my_way=0x1", "amfi_get_out_of_my_way=0x1"},
      {"-s ipc_control_port_options=1", "ipc_control_port_options=1"},
      {"amfi=0x7 ipc_control_port_options=1",
       "amfi=0x7 ipc_control_port_options=1"},
      {"not_amfi=200 amfi_unrestrict_task_for_pid=1",
       "amfi_unrestrict_task_for_pid=1"},
      {"not_amfi=1 -x -v", ""},
  };

  for (const auto& testcase : kCases) {
    EXPECT_EQ(testcase.output, ParseBootArgs(testcase.input));
  }
}

}  // namespace content
