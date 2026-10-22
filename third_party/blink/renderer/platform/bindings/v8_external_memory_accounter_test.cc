// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink::bindings {
namespace {

class V8ExternalMemoryAccounterTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(V8ExternalMemoryAccounterTest, Set_Shrinking) {
  V8ExternalMemoryAccounter accounter;

  const int64_t initial = V8ExternalMemoryAccounterBase::
      GetTotalAmountOfExternalAllocatedMemoryForTesting(
          task_environment_.isolate());
  accounter.Set(task_environment_.isolate(), 1024u);
  const int64_t after_first_set =
      V8ExternalMemoryAccounterBase::
          GetTotalAmountOfExternalAllocatedMemoryForTesting(
              task_environment_.isolate()) -
      initial;
  EXPECT_EQ(after_first_set, 1024);

  accounter.Set(task_environment_.isolate(), 512u);
  const int64_t after_second_set =
      V8ExternalMemoryAccounterBase::
          GetTotalAmountOfExternalAllocatedMemoryForTesting(
              task_environment_.isolate()) -
      initial;
  EXPECT_EQ(after_second_set, 512);

  accounter.Clear(task_environment_.isolate());
}

}  // namespace
}  // namespace blink::bindings
