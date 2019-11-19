// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binder_map.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/tests/binder_map_unittest.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace binder_map_unittest {

class BinderMapTest : public testing::Test {
 public:
  BinderMapTest() = default;

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(BinderMapTest);
};

class TestInterface1Impl : public mojom::TestInterface1 {
 public:
  TestInterface1Impl() = default;
  ~TestInterface1Impl() override = default;

  void Bind(scoped_refptr<base::SequencedTaskRunner> expected_task_runner,
            mojo::PendingReceiver<mojom::TestInterface1> receiver) {
    if (expected_task_runner)
      EXPECT_TRUE(expected_task_runner->RunsTasksInCurrentSequence());
    receiver_.Bind(std::move(receiver));
  }

  void BindFromRequest(
      scoped_refptr<base::SequencedTaskRunner> expected_task_runner,
      mojom::TestInterface1Request request) {
    if (expected_task_runner)
      EXPECT_TRUE(expected_task_runner->RunsTasksInCurrentSequence());
    receiver_.Bind(std::move(request));
  }

 private:
  mojo::Receiver<mojom::TestInterface1> receiver_{this};
};

class TestInterface2Impl : public mojom::TestInterface2 {
 public:
  TestInterface2Impl() = default;
  ~TestInterface2Impl() override = default;

  void Bind(scoped_refptr<base::SequencedTaskRunner> expected_task_runner,
            mojo::PendingReceiver<mojom::TestInterface2> receiver) {
    if (expected_task_runner)
      EXPECT_TRUE(expected_task_runner->RunsTasksInCurrentSequence());
    receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Receiver<mojom::TestInterface2> receiver_{this};
};

TEST_F(BinderMapTest, NoMatch) {
  Remote<mojom::TestInterface1> remote;
  GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());
  BinderMap empty_map;
  EXPECT_FALSE(empty_map.CanBind(receiver));
  EXPECT_FALSE(empty_map.Bind(&receiver));
}

TEST_F(BinderMapTest, BasicMatch) {
  Remote<mojom::TestInterface1> remote;
  GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());

  TestInterface1Impl impl;
  BinderMap map;
  map.Add(base::BindRepeating(&TestInterface1Impl::Bind,
                              base::Unretained(&impl), nullptr),
          base::SequencedTaskRunnerHandle::Get());
  EXPECT_TRUE(map.CanBind(receiver));
  EXPECT_TRUE(map.Bind(&receiver));
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

TEST_F(BinderMapTest, BasicMatchWithInterfaceRequestCallback) {
  Remote<mojom::TestInterface1> remote;
  GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());

  TestInterface1Impl impl;
  BinderMap map;
  map.Add(base::BindRepeating(&TestInterface1Impl::BindFromRequest,
                              base::Unretained(&impl), nullptr),
          base::SequencedTaskRunnerHandle::Get());
  EXPECT_TRUE(map.CanBind(receiver));
  EXPECT_TRUE(map.Bind(&receiver));
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

TEST_F(BinderMapTest, CorrectSequence) {
  Remote<mojom::TestInterface1> remote1;
  GenericPendingReceiver receiver1(remote1.BindNewPipeAndPassReceiver());

  Remote<mojom::TestInterface2> remote2;
  GenericPendingReceiver receiver2(remote2.BindNewPipeAndPassReceiver());

  auto task_runner1 = base::CreateSequencedTaskRunner({base::CurrentThread()});
  auto task_runner2 = base::CreateSequencedTaskRunner({base::ThreadPool()});

  TestInterface1Impl impl1;
  std::unique_ptr<TestInterface2Impl> impl2;

  // Create |impl2| on the ThreadPool task runner.
  base::RunLoop create_impl2_loop;
  task_runner2->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                           impl2 = std::make_unique<TestInterface2Impl>();
                           create_impl2_loop.Quit();
                         }));
  create_impl2_loop.Run();

  BinderMap map;
  map.Add(base::BindRepeating(&TestInterface1Impl::Bind,
                              base::Unretained(&impl1), task_runner1),
          task_runner1);
  map.Add(base::BindRepeating(&TestInterface2Impl::Bind,
                              base::Unretained(impl2.get()), task_runner2),
          task_runner2);
  EXPECT_TRUE(map.Bind(&receiver1));
  EXPECT_TRUE(map.Bind(&receiver2));
  remote1.FlushForTesting();
  remote2.FlushForTesting();
  EXPECT_TRUE(remote1.is_connected());
  EXPECT_TRUE(remote2.is_connected());

  // Destroy |impl2| on the ThreadPool task runner.
  base::RunLoop destroy_impl2_loop;
  task_runner2->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                           impl2.reset();
                           destroy_impl2_loop.Quit();
                         }));
  destroy_impl2_loop.Run();
}

}  // namespace binder_map_unittest
}  // namespace test
}  // namespace mojo
