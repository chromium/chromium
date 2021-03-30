// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"

#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {
struct Foo {
  Foo() = default;
  void Bar(int) {}
  int Baz() { return 42; }
};
}  // namespace

class SequenceBoundTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SequenceBoundTest, CanInstantiate) {
  SequenceBound<Foo> sequence_bound(
      base::ThreadPool::CreateSingleThreadTaskRunner({}));

  sequence_bound.AsyncCall(&Foo::Bar).WithArgs(5);
  sequence_bound.PostTaskWithThisObject(FROM_HERE,
                                        CrossThreadBindOnce([](Foo* foo) {}));
}

}  // namespace WTF
