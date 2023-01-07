// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"

#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {

constexpr int kTestValue = 42;

struct Foo {
  Foo() = default;
  void Bar(int) {}
  int Baz() { return kTestValue; }
};

}  // namespace

class SequenceBoundTest : public testing::Test {
 public:
  void CheckValue(base::RunLoop* run_loop, int* dest_value, int value) {
    *dest_value = value;
    run_loop->Quit();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SequenceBoundTest, CanInstantiate) {
  SequenceBound<Foo> sequence_bound(
      base::ThreadPool::CreateSingleThreadTaskRunner({}));

  sequence_bound.AsyncCall(&Foo::Bar).WithArgs(5);
  sequence_bound.PostTaskWithThisObject(CrossThreadBindOnce([](Foo* foo) {}));

  int test_value = -1;
  base::RunLoop run_loop;
  sequence_bound.AsyncCall(&Foo::Baz).Then(CrossThreadBindOnce(
      &SequenceBoundTest::CheckValue, CrossThreadUnretained(this),
      CrossThreadUnretained(&run_loop), CrossThreadUnretained(&test_value)));
  run_loop.Run();
  EXPECT_EQ(test_value, kTestValue);
}

}  // namespace WTF
