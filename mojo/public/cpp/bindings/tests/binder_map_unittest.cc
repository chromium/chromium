// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binder_map.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
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

  BinderMapTest(const BinderMapTest&) = delete;
  BinderMapTest& operator=(const BinderMapTest&) = delete;

 private:
  base::test::TaskEnvironment task_environment_;
};

class TestInterface1Impl : public mojom::TestInterface1 {
 public:
  explicit TestInterface1Impl(base::OnceClosure destruction_callback = {})
      : destruction_callback_(std::move(destruction_callback)) {}

  TestInterface1Impl(const TestInterface1Impl&) = delete;
  TestInterface1Impl& operator=(const TestInterface1Impl&) = delete;

  ~TestInterface1Impl() override {
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

  void Bind(scoped_refptr<base::SequencedTaskRunner> expected_task_runner,
            mojo::PendingReceiver<mojom::TestInterface1> receiver) {
    if (expected_task_runner)
      EXPECT_TRUE(expected_task_runner->RunsTasksInCurrentSequence());
    receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Receiver<mojom::TestInterface1> receiver_{this};
  base::OnceClosure destruction_callback_;
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

namespace {
// Used by binder functors to set a callback which is run on binder destruction.
// This is to allow tests to wait for a binder to be destroyed. We can't use
// base::BindRepeating, as we are explicitly testing adding a function pointer,
// as opposed to a RepeatingCallback.
base::OnceClosure g_destruction_closure_for_testing;
}  // namespace

void Interface1Functor(mojo::PendingReceiver<mojom::TestInterface1> receiver) {
  MakeSelfOwnedReceiver(std::make_unique<TestInterface1Impl>(
                            std::move(g_destruction_closure_for_testing)),
                        std::move(receiver));
}

void Interface1Functor42(
    int context,
    mojo::PendingReceiver<mojom::TestInterface1> receiver) {
  EXPECT_EQ(context, 42);
  MakeSelfOwnedReceiver(std::make_unique<TestInterface1Impl>(
                            std::move(g_destruction_closure_for_testing)),
                        std::move(receiver));
}

TEST_F(BinderMapTest, NoMatch) {
  Remote<mojom::TestInterface1> remote;
  GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());
  BinderMap empty_map;
  EXPECT_FALSE(empty_map.TryBind(&receiver));
}

TEST_F(BinderMapTest, BasicMatch) {
  Remote<mojom::TestInterface1> remote;
  GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());

  TestInterface1Impl impl;
  BinderMap map;
  map.Add<mojom::TestInterface1>(
      base::BindRepeating(&TestInterface1Impl::Bind, base::Unretained(&impl),
                          nullptr),
      base::SequencedTaskRunner::GetCurrentDefault());
  EXPECT_TRUE(map.TryBind(&receiver));
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

TEST_F(BinderMapTest, BasicMatchWithFunctor) {
  Remote<mojom::TestInterface1> remote;
  GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());

  auto loop = base::RunLoop();
  g_destruction_closure_for_testing = loop.QuitClosure();
  BinderMap map;
  map.Add<mojom::TestInterface1>(
      &Interface1Functor, base::SequencedTaskRunner::GetCurrentDefault());
  EXPECT_TRUE(map.TryBind(&receiver));
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  // Allow the self-owned receiver to be destroyed.
  remote.reset();
  loop.Run();
}

TEST_F(BinderMapTest, WithContext) {
  Remote<mojom::TestInterface1> remote;
  GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());

  int context = 42;
  TestInterface1Impl impl;
  BinderMapWithContext<int*> map;
  map.Add<mojom::TestInterface1>(base::BindRepeating(
      [](TestInterface1Impl* impl, int* expected_context, int* context,
         mojo::PendingReceiver<mojom::TestInterface1> receiver) {
        EXPECT_EQ(context, expected_context);
        impl->Bind(nullptr, std::move(receiver));
      },
      base::Unretained(&impl), base::Unretained(&context)));
  EXPECT_TRUE(map.TryBind(&context, &receiver));
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

TEST_F(BinderMapTest, FunctorWithContext) {
  Remote<mojom::TestInterface1> remote;
  GenericPendingReceiver receiver(remote.BindNewPipeAndPassReceiver());

  auto loop = base::RunLoop();
  g_destruction_closure_for_testing = loop.QuitClosure();
  BinderMapWithContext<int> map;
  map.Add<mojom::TestInterface1>(&Interface1Functor42);
  EXPECT_TRUE(map.TryBind(42, &receiver));
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());

  // Allow the self-owned receiver to be destroyed.
  remote.reset();
  loop.Run();
}

TEST_F(BinderMapTest, CorrectSequence) {
  Remote<mojom::TestInterface1> remote1;
  GenericPendingReceiver receiver1(remote1.BindNewPipeAndPassReceiver());

  Remote<mojom::TestInterface2> remote2;
  GenericPendingReceiver receiver2(remote2.BindNewPipeAndPassReceiver());

  auto task_runner1 = base::SequencedTaskRunner::GetCurrentDefault();
  auto task_runner2 = base::ThreadPool::CreateSequencedTaskRunner({});

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
  map.Add<mojom::TestInterface1>(
      base::BindRepeating(&TestInterface1Impl::Bind, base::Unretained(&impl1),
                          task_runner1),
      task_runner1);
  map.Add<mojom::TestInterface2>(
      base::BindRepeating(&TestInterface2Impl::Bind,
                          base::Unretained(impl2.get()), task_runner2),
      task_runner2);
  EXPECT_TRUE(map.TryBind(&receiver1));
  EXPECT_TRUE(map.TryBind(&receiver2));
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
