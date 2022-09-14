// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/process_id_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/process_id.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace process_id_unittest {

TEST(ProcessIdTest, ProcessId) {
  base::ProcessId pid = base::GetCurrentProcId();
  base::ProcessId out_pid = base::kNullProcessId;
  ASSERT_NE(pid, out_pid);
  EXPECT_TRUE(mojom::ProcessId::Deserialize(mojom::ProcessId::Serialize(&pid),
                                            &out_pid));
  EXPECT_EQ(pid, out_pid);
}

}  // namespace process_id_unittest
}  // namespace mojo_base