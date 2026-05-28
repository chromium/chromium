// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_protection_manager.h"

#include <wrl/client.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "media/base/waiting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class DestructionObserver
    : public base::RefCountedThreadSafe<DestructionObserver> {
 public:
  DestructionObserver(scoped_refptr<base::SequencedTaskRunner> task_runner,
                      bool* destroyed_on_correct_sequence)
      : task_runner_(std::move(task_runner)),
        destroyed_on_correct_sequence_(destroyed_on_correct_sequence) {}

 private:
  friend class base::RefCountedThreadSafe<DestructionObserver>;
  ~DestructionObserver() {
    *destroyed_on_correct_sequence_ =
        task_runner_->RunsTasksInCurrentSequence();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<bool> destroyed_on_correct_sequence_;
};

}  // namespace

class MediaFoundationProtectionManagerTest : public testing::Test {
 public:
  MediaFoundationProtectionManagerTest() = default;
  ~MediaFoundationProtectionManagerTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};
};

TEST_F(MediaFoundationProtectionManagerTest, DestructionOnTaskRunner) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  bool destroyed_on_correct_sequence = false;
  auto observer = base::MakeRefCounted<DestructionObserver>(
      task_runner, &destroyed_on_correct_sequence);

  Microsoft::WRL::ComPtr<MediaFoundationProtectionManager> protection_manager;
  HRESULT hr =
      Microsoft::WRL::MakeAndInitialize<MediaFoundationProtectionManager>(
          &protection_manager, task_runner,
          base::BindRepeating(
              [](scoped_refptr<DestructionObserver>, WaitingReason) {},
              observer));
  EXPECT_TRUE(SUCCEEDED(hr));

  auto wrapper = protection_manager;
  protection_manager.Reset();
  observer.reset();

  base::WaitableEvent event;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Microsoft::WRL::ComPtr<MediaFoundationProtectionManager> wrapper,
             base::WaitableEvent* event) {
            wrapper.Reset();
            event->Signal();
          },
          std::move(wrapper), &event));
  event.Wait();

  // Run the current thread's loop to execute the DeleteSoon task.
  task_environment_.RunUntilIdle();

  // This fails without the Use-After-Free fix for
  // MediaFoundationProtectionManager.
  EXPECT_TRUE(destroyed_on_correct_sequence);
}

}  // namespace media
