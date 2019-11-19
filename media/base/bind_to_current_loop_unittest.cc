// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bind_to_current_loop.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/free_deleter.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

void BoundBoolSet(bool* var, bool val) {
  *var = val;
}

void BoundBoolSetFromUniquePtr(bool* var, std::unique_ptr<bool> val) {
  *var = *val;
}

void BoundBoolSetFromUniquePtrFreeDeleter(
    bool* var,
    std::unique_ptr<bool, base::FreeDeleter> val) {
  *var = *val;
}

void BoundBoolSetFromUniquePtrArray(bool* var, std::unique_ptr<bool[]> val) {
  *var = val[0];
}

void BoundBoolSetFromConstRef(bool* var, const bool& val) {
  *var = val;
}

void BoundIntegersSet(int* a_var, int* b_var, int a_val, int b_val) {
  *a_var = a_val;
  *b_var = b_val;
}

struct ThreadRestrictionChecker {
  void Run() { EXPECT_TRUE(thread_checker_.CalledOnValidThread()); }

  ~ThreadRestrictionChecker() {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
  }

  base::ThreadCheckerImpl thread_checker_;
};

void ClearReference(base::OnceClosure cb) {}

// Various tests that check that the bound function is only actually executed
// on the message loop, not during the original Run.
class BindToCurrentLoopTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(BindToCurrentLoopTest, RepeatingClosure) {
  // Test the closure is run inside the loop, not outside it.
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::RepeatingClosure cb = BindToCurrentLoop(base::BindRepeating(
      &base::WaitableEvent::Signal, base::Unretained(&waiter)));
  cb.Run();
  EXPECT_FALSE(waiter.IsSignaled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(waiter.IsSignaled());
}

TEST_F(BindToCurrentLoopTest, OnceClosure) {
  // Test the closure is run inside the loop, not outside it.
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::OnceClosure cb = BindToCurrentLoop(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
  std::move(cb).Run();
  EXPECT_FALSE(waiter.IsSignaled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(waiter.IsSignaled());
}

TEST_F(BindToCurrentLoopTest, BoolRepeating) {
  bool bool_var = false;
  base::RepeatingCallback<void(bool)> cb =
      BindToCurrentLoop(base::BindRepeating(&BoundBoolSet, &bool_var));
  cb.Run(true);
  EXPECT_FALSE(bool_var);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_var);

  cb.Run(false);
  EXPECT_TRUE(bool_var);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bool_var);
}

TEST_F(BindToCurrentLoopTest, BoolOnce) {
  bool bool_var = false;
  base::OnceCallback<void(bool)> cb =
      BindToCurrentLoop(base::BindOnce(&BoundBoolSet, &bool_var));
  std::move(cb).Run(true);
  EXPECT_FALSE(bool_var);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_var);
}

TEST_F(BindToCurrentLoopTest, BoundUniquePtrBoolRepeating) {
  bool bool_val = false;
  std::unique_ptr<bool> unique_ptr_bool(new bool(true));
  base::RepeatingClosure cb = BindToCurrentLoop(base::BindRepeating(
      &BoundBoolSetFromUniquePtr, &bool_val, base::Passed(&unique_ptr_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, BoundUniquePtrBoolOnce) {
  bool bool_val = false;
  std::unique_ptr<bool> unique_ptr_bool(new bool(true));
  base::OnceClosure cb = BindToCurrentLoop(base::BindOnce(
      &BoundBoolSetFromUniquePtr, &bool_val, std::move(unique_ptr_bool)));
  std::move(cb).Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedUniquePtrBoolRepeating) {
  bool bool_val = false;
  base::RepeatingCallback<void(std::unique_ptr<bool>)> cb = BindToCurrentLoop(
      base::BindRepeating(&BoundBoolSetFromUniquePtr, &bool_val));
  cb.Run(std::make_unique<bool>(true));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);

  cb.Run(std::make_unique<bool>(false));
  EXPECT_TRUE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedUniquePtrBoolOnce) {
  bool bool_val = false;
  base::OnceCallback<void(std::unique_ptr<bool>)> cb =
      BindToCurrentLoop(base::BindOnce(&BoundBoolSetFromUniquePtr, &bool_val));
  std::move(cb).Run(std::make_unique<bool>(true));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, BoundUniquePtrArrayBoolRepeating) {
  bool bool_val = false;
  std::unique_ptr<bool[]> unique_ptr_array_bool(new bool[1]);
  unique_ptr_array_bool[0] = true;
  base::RepeatingClosure cb = BindToCurrentLoop(
      base::BindRepeating(&BoundBoolSetFromUniquePtrArray, &bool_val,
                          base::Passed(&unique_ptr_array_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, BoundUniquePtrArrayBoolOnce) {
  bool bool_val = false;
  std::unique_ptr<bool[]> unique_ptr_array_bool(new bool[1]);
  unique_ptr_array_bool[0] = true;
  base::OnceClosure cb = BindToCurrentLoop(
      base::BindOnce(&BoundBoolSetFromUniquePtrArray, &bool_val,
                     std::move(unique_ptr_array_bool)));
  std::move(cb).Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedUniquePtrArrayBoolRepeating) {
  bool bool_val = false;
  base::RepeatingCallback<void(std::unique_ptr<bool[]>)> cb = BindToCurrentLoop(
      base::BindRepeating(&BoundBoolSetFromUniquePtrArray, &bool_val));

  std::unique_ptr<bool[]> unique_ptr_array_bool(new bool[1]);
  unique_ptr_array_bool[0] = true;
  cb.Run(std::move(unique_ptr_array_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);

  unique_ptr_array_bool.reset(new bool[1]);
  unique_ptr_array_bool[0] = false;
  cb.Run(std::move(unique_ptr_array_bool));
  EXPECT_TRUE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedUniquePtrArrayBoolOnce) {
  bool bool_val = false;
  base::OnceCallback<void(std::unique_ptr<bool[]>)> cb = BindToCurrentLoop(
      base::BindOnce(&BoundBoolSetFromUniquePtrArray, &bool_val));

  std::unique_ptr<bool[]> unique_ptr_array_bool(new bool[1]);
  unique_ptr_array_bool[0] = true;
  std::move(cb).Run(std::move(unique_ptr_array_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, BoundUniquePtrFreeDeleterBoolRepeating) {
  bool bool_val = false;
  std::unique_ptr<bool, base::FreeDeleter> unique_ptr_free_deleter_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *unique_ptr_free_deleter_bool = true;
  base::RepeatingClosure cb = BindToCurrentLoop(
      base::BindRepeating(&BoundBoolSetFromUniquePtrFreeDeleter, &bool_val,
                          base::Passed(&unique_ptr_free_deleter_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, BoundUniquePtrFreeDeleterBoolOnce) {
  bool bool_val = false;
  std::unique_ptr<bool, base::FreeDeleter> unique_ptr_free_deleter_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *unique_ptr_free_deleter_bool = true;
  base::OnceClosure cb = BindToCurrentLoop(
      base::BindOnce(&BoundBoolSetFromUniquePtrFreeDeleter, &bool_val,
                     std::move(unique_ptr_free_deleter_bool)));
  std::move(cb).Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedUniquePtrFreeDeleterBoolRepeating) {
  bool bool_val = false;
  base::RepeatingCallback<void(std::unique_ptr<bool, base::FreeDeleter>)> cb =
      BindToCurrentLoop(base::BindRepeating(
          &BoundBoolSetFromUniquePtrFreeDeleter, &bool_val));

  std::unique_ptr<bool, base::FreeDeleter> unique_ptr_free_deleter_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *unique_ptr_free_deleter_bool = true;
  cb.Run(std::move(unique_ptr_free_deleter_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);

  unique_ptr_free_deleter_bool.reset(static_cast<bool*>(malloc(sizeof(bool))));
  *unique_ptr_free_deleter_bool = false;
  cb.Run(std::move(unique_ptr_free_deleter_bool));
  EXPECT_TRUE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedUniquePtrFreeDeleterBoolOnce) {
  bool bool_val = false;
  base::OnceCallback<void(std::unique_ptr<bool, base::FreeDeleter>)> cb =
      BindToCurrentLoop(
          base::BindOnce(&BoundBoolSetFromUniquePtrFreeDeleter, &bool_val));

  std::unique_ptr<bool, base::FreeDeleter> unique_ptr_free_deleter_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *unique_ptr_free_deleter_bool = true;
  std::move(cb).Run(std::move(unique_ptr_free_deleter_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, IntegersRepeating) {
  int a = 0;
  int b = 0;
  base::RepeatingCallback<void(int, int)> cb =
      BindToCurrentLoop(base::BindRepeating(&BoundIntegersSet, &a, &b));
  cb.Run(1, -1);
  EXPECT_EQ(a, 0);
  EXPECT_EQ(b, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, -1);

  cb.Run(2, -2);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, -1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(a, 2);
  EXPECT_EQ(b, -2);
}

TEST_F(BindToCurrentLoopTest, IntegersOnce) {
  int a = 0;
  int b = 0;
  base::OnceCallback<void(int, int)> cb =
      BindToCurrentLoop(base::BindOnce(&BoundIntegersSet, &a, &b));
  std::move(cb).Run(1, -1);
  EXPECT_EQ(a, 0);
  EXPECT_EQ(b, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, -1);
}

TEST_F(BindToCurrentLoopTest, DestroyedOnBoundLoopRepeating) {
  base::Thread target_thread("testing");
  ASSERT_TRUE(target_thread.Start());

  // Ensure that the bound object is also destroyed on the correct thread even
  // if the last reference to the callback is dropped on the other thread.
  base::RepeatingClosure cb = BindToCurrentLoop(
      base::BindRepeating(&ThreadRestrictionChecker::Run,
                          std::make_unique<ThreadRestrictionChecker>()));
  target_thread.task_runner()->PostTask(FROM_HERE, std::move(cb));
  ASSERT_FALSE(cb);
  target_thread.FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Ensure that the bound object is destroyed on the target thread even if
  // the callback is destroyed without invocation.
  cb = BindToCurrentLoop(
      base::BindRepeating(&ThreadRestrictionChecker::Run,
                          std::make_unique<ThreadRestrictionChecker>()));
  target_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ClearReference, std::move(cb)));
  target_thread.FlushForTesting();
  ASSERT_FALSE(cb);
  base::RunLoop().RunUntilIdle();

  target_thread.Stop();
}

TEST_F(BindToCurrentLoopTest, DestroyedOnBoundLoopOnce) {
  base::Thread target_thread("testing");
  ASSERT_TRUE(target_thread.Start());

  // Ensure that the bound object is also destroyed on the correct thread even
  // if the last reference to the callback is dropped on the other thread.
  base::OnceClosure cb = BindToCurrentLoop(
      base::BindOnce(&ThreadRestrictionChecker::Run,
                     std::make_unique<ThreadRestrictionChecker>()));
  target_thread.task_runner()->PostTask(FROM_HERE, std::move(cb));
  ASSERT_FALSE(cb);
  target_thread.FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Ensure that the bound object is destroyed on the target thread even if
  // the callback is destroyed without invocation.
  cb = BindToCurrentLoop(
      base::BindOnce(&ThreadRestrictionChecker::Run,
                     std::make_unique<ThreadRestrictionChecker>()));
  target_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ClearReference, std::move(cb)));
  target_thread.FlushForTesting();
  ASSERT_FALSE(cb);
  base::RunLoop().RunUntilIdle();

  target_thread.Stop();
}

}  // namespace media
