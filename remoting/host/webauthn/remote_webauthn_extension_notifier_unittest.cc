// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_extension_notifier.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

void AssertExtensionIdWakeupFilesExist(const base::FilePath& path_prefix) {
  for (const auto& id :
       RemoteWebAuthnExtensionNotifier::GetRemoteWebAuthnExtensionIds()) {
    ASSERT_TRUE(base::PathExists(path_prefix.Append(id)));
  }
}

void AssertExtensionIdWakeupFilesNotExist(const base::FilePath& path_prefix) {
  for (const auto& id :
       RemoteWebAuthnExtensionNotifier::GetRemoteWebAuthnExtensionIds()) {
    ASSERT_FALSE(base::PathExists(path_prefix.Append(id)));
  }
}

void DeleteExtensionIdWakeupFiles(const base::FilePath& path_prefix) {
  for (const auto& id :
       RemoteWebAuthnExtensionNotifier::GetRemoteWebAuthnExtensionIds()) {
    base::FilePath file_path = path_prefix.Append(id);
    base::DeleteFile(file_path);
  }
}

}  // namespace

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
  EXPECT_FALSE(
      RemoteWebAuthnExtensionNotifier::GetRemoteWebAuthnExtensionIds().empty());
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
  AssertExtensionIdWakeupFilesExist(scoped_temp_dir_1_.GetPath());
  AssertExtensionIdWakeupFilesExist(scoped_temp_dir_2_.GetPath());
}

TEST_F(RemoteWebAuthnExtensionNotifierTest,
       WritesFilesOnlyInExistingDirectory) {
  base::FilePath deleted_temp_dir_1_path = scoped_temp_dir_1_.GetPath();
  ASSERT_TRUE(scoped_temp_dir_1_.Delete());
  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  AssertExtensionIdWakeupFilesNotExist(deleted_temp_dir_1_path);
  AssertExtensionIdWakeupFilesExist(scoped_temp_dir_2_.GetPath());
}

TEST_F(RemoteWebAuthnExtensionNotifierTest, FilesRewrittenAfterSecondCall) {
  base::FilePath wakeup_file_prefix = scoped_temp_dir_1_.GetPath();

  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  AssertExtensionIdWakeupFilesExist(wakeup_file_prefix);

  DeleteExtensionIdWakeupFiles(wakeup_file_prefix);
  AssertExtensionIdWakeupFilesNotExist(wakeup_file_prefix);

  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  AssertExtensionIdWakeupFilesExist(wakeup_file_prefix);
}

TEST_F(RemoteWebAuthnExtensionNotifierTest,
       FileWrittenWhenNotifierIsDeletedRightAfterNotifyStateChangeIsCalled) {
  notifier_->NotifyStateChange();
  notifier_.reset();
  WaitForNextIoTask();
  AssertExtensionIdWakeupFilesExist(scoped_temp_dir_1_.GetPath());
}

TEST_F(RemoteWebAuthnExtensionNotifierTest,
       OnlyOneExtensionWakeupTaskIsScheduledForMultipleCalls) {
  base::FilePath wakeup_file_prefix = scoped_temp_dir_1_.GetPath();
  notifier_->NotifyStateChange();
  notifier_->NotifyStateChange();
  notifier_->NotifyStateChange();
  WaitForNextIoTask();
  AssertExtensionIdWakeupFilesExist(wakeup_file_prefix);

  DeleteExtensionIdWakeupFiles(wakeup_file_prefix);
  AssertExtensionIdWakeupFilesNotExist(wakeup_file_prefix);

  WaitForNextIoTask();
  WaitForNextIoTask();
  for (const auto& id :
       RemoteWebAuthnExtensionNotifier::GetRemoteWebAuthnExtensionIds()) {
    ASSERT_FALSE(base::PathExists(wakeup_file_prefix.Append(id)));
  }
}

}  // namespace remoting
