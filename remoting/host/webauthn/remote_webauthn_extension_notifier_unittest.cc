// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_extension_notifier.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class RemoteWebAuthnExtensionNotifierTest : public testing::Test {
 public:
  RemoteWebAuthnExtensionNotifierTest();
  ~RemoteWebAuthnExtensionNotifierTest() override;

 protected:
  void WaitForNextIoTask();

  base::ScopedTempDir scoped_temp_dir_1_;
  base::ScopedTempDir scoped_temp_dir_2_;
  std::unique_ptr<RemoteWebAuthnExtensionNotifier> notifier_;

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};

RemoteWebAuthnExtensionNotifierTest::RemoteWebAuthnExtensionNotifierTest() {
  EXPECT_TRUE(scoped_temp_dir_1_.CreateUniqueTempDir());
  EXPECT_TRUE(scoped_temp_dir_2_.CreateUniqueTempDir());
  io_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives()});
  notifier_ = base::WrapUnique(new RemoteWebAuthnExtensionNotifier(
      {
          scoped_temp_dir_1_.GetPath(),
          scoped_temp_dir_2_.GetPath(),
      },
      io_task_runner_));
}

RemoteWebAuthnExtensionNotifierTest::~RemoteWebAuthnExtensionNotifierTest() {
  notifier_.reset();
  // Wait for the notifier core to be deleted on the IO sequence so that ASAN
  // doesn't complain about leaked memory.
  WaitForNextIoTask();
}

void RemoteWebAuthnExtensionNotifierTest::WaitForNextIoTask() {
  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        io_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                          run_loop.QuitClosure());
      }));
  run_loop.Run();
}

TEST_F(RemoteWebAuthnExtensionNotifierTest, WritesFileInBothDirectories) {
  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  ASSERT_TRUE(base::PathExists(scoped_temp_dir_1_.GetPath().Append(
      RemoteWebAuthnExtensionNotifier::kRemoteWebAuthnExtensionId)));
  ASSERT_TRUE(base::PathExists(scoped_temp_dir_2_.GetPath().Append(
      RemoteWebAuthnExtensionNotifier::kRemoteWebAuthnExtensionId)));
}

TEST_F(RemoteWebAuthnExtensionNotifierTest, WritesFileOnlyInExistingDirectory) {
  base::FilePath deleted_temp_dir_1_path = scoped_temp_dir_1_.GetPath();
  ASSERT_TRUE(scoped_temp_dir_1_.Delete());
  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  ASSERT_FALSE(base::PathExists(deleted_temp_dir_1_path.Append(
      RemoteWebAuthnExtensionNotifier::kRemoteWebAuthnExtensionId)));
  ASSERT_TRUE(base::PathExists(scoped_temp_dir_2_.GetPath().Append(
      RemoteWebAuthnExtensionNotifier::kRemoteWebAuthnExtensionId)));
}

TEST_F(RemoteWebAuthnExtensionNotifierTest, FileRewrittenAfterSecondCall) {
  base::FilePath wakeup_file_path = scoped_temp_dir_1_.GetPath().Append(
      RemoteWebAuthnExtensionNotifier::kRemoteWebAuthnExtensionId);

  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  ASSERT_TRUE(base::PathExists(wakeup_file_path));

  base::DeleteFile(wakeup_file_path);
  ASSERT_FALSE(base::PathExists(wakeup_file_path));

  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  ASSERT_TRUE(base::PathExists(wakeup_file_path));
}

TEST_F(RemoteWebAuthnExtensionNotifierTest,
       FileWrittenWhenNotifierIsDeletedRightAfterNotifyStateChangeIsCalled) {
  notifier_->NotifyStateChange();
  notifier_.reset();
  WaitForNextIoTask();
  ASSERT_TRUE(base::PathExists(scoped_temp_dir_1_.GetPath().Append(
      RemoteWebAuthnExtensionNotifier::kRemoteWebAuthnExtensionId)));
}

TEST_F(RemoteWebAuthnExtensionNotifierTest,
       OnlyOneExtensionWakeupTaskIsScheduledForMultipleCalls) {
  base::FilePath wakeup_file_path = scoped_temp_dir_1_.GetPath().Append(
      RemoteWebAuthnExtensionNotifier::kRemoteWebAuthnExtensionId);
  notifier_->NotifyStateChange();
  notifier_->NotifyStateChange();
  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  ASSERT_TRUE(base::PathExists(wakeup_file_path));

  base::DeleteFile(wakeup_file_path);
  ASSERT_FALSE(base::PathExists(wakeup_file_path));

  WaitForNextIoTask();
  WaitForNextIoTask();
  ASSERT_FALSE(base::PathExists(wakeup_file_path));
}

}  // namespace remoting
