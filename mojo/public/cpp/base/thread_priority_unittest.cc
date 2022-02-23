// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/thread_priority_mojom_traits.h"

#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace thread_priority_unittest {

TEST(ThreadPriorityTest, ThreadPriority) {
  static constexpr base::ThreadPriority kTestPriorities[] = {
      base::ThreadPriority::BACKGROUND, base::ThreadPriority::NORMAL,
      base::ThreadPriority::DISPLAY, base::ThreadPriority::REALTIME_AUDIO};

  for (auto priority_in : kTestPriorities) {
    base::ThreadPriority priority_out;

    mojo_base::mojom::ThreadPriority serialized_priority =
        mojo::EnumTraits<mojo_base::mojom::ThreadPriority,
                         base::ThreadPriority>::ToMojom(priority_in);
    ASSERT_TRUE(
        (mojo::EnumTraits<mojo_base::mojom::ThreadPriority,
                          base::ThreadPriority>::FromMojom(serialized_priority,
                                                           &priority_out)));
    EXPECT_EQ(priority_in, priority_out);
  }
}

}  // namespace thread_priority_unittest
}  // namespace mojo_base
