// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/dom_storage/local_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace content {

class DOMStorageContextWrapperTest : public testing::Test {
 public:
  DOMStorageContextWrapperTest() = default;

  void SetUp() override {
    storage_policy_ = new MockSpecialStoragePolicy();
    fake_mojo_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    auto* session_storage_context = new SessionStorageContextMojo(
        /*partition_path=*/base::FilePath(),
        base::CreateSequencedTaskRunner(
            {base::MayBlock(), base::ThreadPool(),
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
        fake_mojo_task_runner_, SessionStorageContextMojo::BackingMode::kNoDisk,
        /*leveldb_name=*/"");
    session_storage_context->PretendToConnectForTesting();
    context_ = new DOMStorageContextWrapper(
        fake_mojo_task_runner_, /*mojo_local_storage_context=*/nullptr,
        session_storage_context);
  }

  void TearDown() override {
    context_->Shutdown();
    context_.reset();
    fake_mojo_task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> fake_mojo_task_runner_;
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<DOMStorageContextWrapper> context_;

  DISALLOW_COPY_AND_ASSIGN(DOMStorageContextWrapperTest);
};

TEST_F(DOMStorageContextWrapperTest, BadMessageScheduling) {
  // This is a regression test for https://crbug.com/916523, which verifies that
  // when SessionStorageContextMojo invokes its bad-message callback on the
  // main task runner rather than SessionStorageContextMojo's internal task
  // runner. This is necessary because the bad-message callback is associated
  // with StoragePartitionImpl's ReceiverSet which lives on the main thread.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace_remote;
  bool called = false;
  // This call is invalid because |CreateSessionNamespace| was never called on
  // the SessionStorage context.
  context_->OpenSessionStorage(
      0, "nonexistant-namespace",
      base::BindLambdaForTesting(
          [&called](const std::string& message) { called = true; }),
      ss_namespace_remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(called);
  fake_mojo_task_runner_->RunPendingTasks();

  // There should not be an error yet, as the callback has to run on
  // 'this' task runner and not the fake one.
  EXPECT_FALSE(called);

  // Cycle the current task runner.
  base::RunLoop loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   loop.QuitClosure());
  loop.Run();

  // The callback should have run now.
  EXPECT_TRUE(called);
}

}  // namespace content
