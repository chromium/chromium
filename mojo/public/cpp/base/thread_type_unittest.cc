// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/thread_type_mojom_traits.h"

#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base::thread_type_unittest {

TEST(ThreadTypeTest, ThreadType) {
  static constexpr base::ThreadType kTestTypes[] = {
      base::ThreadType::kBackground, base::ThreadType::kDefault,
      base::ThreadType::kDisplayCritical, base::ThreadType::kRealtimeAudio};

  for (auto thread_type_in : kTestTypes) {
    base::ThreadType thread_type_out;

    mojo_base::mojom::ThreadType serialized_thread_type =
        mojo::EnumTraits<mojo_base::mojom::ThreadType,
                         base::ThreadType>::ToMojom(thread_type_in);
    ASSERT_TRUE(
        (mojo::EnumTraits<mojo_base::mojom::ThreadType,
                          base::ThreadType>::FromMojom(serialized_thread_type,
                                                       &thread_type_out)));
    EXPECT_EQ(thread_type_in, thread_type_out);
  }
}

}  // namespace mojo_base::thread_type_unittest
