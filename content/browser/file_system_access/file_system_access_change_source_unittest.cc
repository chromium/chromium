// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_change_source.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

namespace content {

namespace {

class MockRawChangeObserver
    : public FileSystemAccessChangeSource::RawChangeObserver {
 public:
  MOCK_METHOD(void,
              OnRawChange,
              (const storage::FileSystemURL& changed_url,
               bool error,
               const FileSystemAccessChangeSource::ChangeInfo& change_info,
               FileSystemAccessWatchScope scope),
              (override));
  MOCK_METHOD(void,
              OnUsageChange,
              (size_t old_usage,
               size_t new_usage,
               FileSystemAccessWatchScope scope),
              (override));
  MOCK_METHOD(void,
              OnSourceBeingDestroyed,
              (FileSystemAccessChangeSource * source),
              (override));
};

class FakeChangeSource : public FileSystemAccessChangeSource {
 public:
  FakeChangeSource(
      FileSystemAccessWatchScope scope,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      blink::mojom::FileSystemAccessStatus initialization_status =
          blink::mojom::FileSystemAccessStatus::kOk)
      : FileSystemAccessChangeSource(std::move(scope),
                                     std::move(file_system_context)),
        initialization_status_(initialization_status) {}
  ~FakeChangeSource() override = default;

  // FileSystemAccessChangeSource:
  void Initialize(
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
          on_source_initialized) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_source_initialized),
                       blink::mojom::FileSystemAccessError::New(
                           initialization_status_,
                           initialization_status_ ==
                                   blink::mojom::FileSystemAccessStatus::kOk
                               ? base::File::FILE_OK
                               : base::File::FILE_ERROR_FAILED,
                           "")));
  }

  void Signal(const storage::FileSystemURL& changed_url,
              bool error = false,
              ChangeInfo change_info = ChangeInfo()) {
    NotifyOfChange(changed_url, error, change_info);
  }

  void Signal(const base::FilePath& relative_path) {
    NotifyOfChange(relative_path, /*error=*/false, ChangeInfo());
  }

  void SignalUsageChange(size_t old_usage, size_t new_usage) {
    NotifyOfUsageChange(old_usage, new_usage);
  }

 private:
  const blink::mojom::FileSystemAccessStatus initialization_status_;
};

}  // namespace

class FileSystemAccessChangeSourceTest : public testing::Test {
 public:
  FileSystemAccessChangeSourceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(FileSystemAccessChangeSourceTest, CreateAndInitialize) {
  auto file_path = dir_.GetPath().AppendASCII("file");
  auto file_url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal, file_path);

  auto scope = FileSystemAccessWatchScope::GetScopeForFileWatch(file_url);
  FakeChangeSource source(scope, file_system_context_);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  source.EnsureInitialized(future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
}

TEST_F(FileSystemAccessChangeSourceTest, NotifyOfChange) {
  auto file_path = dir_.GetPath().AppendASCII("file");
  auto file_url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal, file_path);

  auto scope = FileSystemAccessWatchScope::GetScopeForFileWatch(file_url);
  FakeChangeSource source(scope, file_system_context_);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  source.EnsureInitialized(future.GetCallback());
  ASSERT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);

  MockRawChangeObserver observer;
  source.AddObserver(&observer);

  EXPECT_CALL(observer, OnRawChange(testing::Eq(file_url), testing::IsFalse(),
                                    testing::_, testing::Eq(scope)))
      .Times(2);
  source.Signal(file_url);
  source.Signal(base::FilePath());
  EXPECT_CALL(observer, OnUsageChange(1, 2, testing::Eq(scope)));
  source.SignalUsageChange(1, 2);

  source.RemoveObserver(&observer);
}

TEST_F(FileSystemAccessChangeSourceTest, NotifyOfChangeDuringInitialization) {
  auto file_url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal,
      dir_.GetPath().AppendASCII("file"));
  auto scope = FileSystemAccessWatchScope::GetScopeForFileWatch(file_url);
  FakeChangeSource source(scope, file_system_context_);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  source.EnsureInitialized(future.GetCallback());
  ASSERT_FALSE(future.IsReady());

  MockRawChangeObserver observer;
  source.AddObserver(&observer);
  EXPECT_CALL(observer, OnRawChange(testing::Eq(file_url), testing::IsFalse(),
                                    testing::_, testing::Eq(scope)))
      .Times(2);
  source.Signal(file_url);
  source.Signal(base::FilePath());
  EXPECT_CALL(observer, OnUsageChange(1, 2, testing::Eq(scope)));
  source.SignalUsageChange(1, 2);
  source.RemoveObserver(&observer);

  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
}

// Milestone CHECKs can be non-fatal in official builds, so only run these death
// tests in non-official builds.
#if !defined(OFFICIAL_BUILD)  // nocheck
using FileSystemAccessChangeSourceDeathTest = FileSystemAccessChangeSourceTest;

TEST_F(FileSystemAccessChangeSourceDeathTest,
       NotifyOfChangeBeforeInitialization) {
  auto file_url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal,
      dir_.GetPath().AppendASCII("file"));
  FakeChangeSource source(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url),
      file_system_context_);

  EXPECT_CHECK_DEATH_WITH(source.Signal(file_url), "can_notify");
}

TEST_F(FileSystemAccessChangeSourceDeathTest,
       NotifyOfRelativePathChangeBeforeInitialization) {
  auto file_url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal,
      dir_.GetPath().AppendASCII("file"));
  FakeChangeSource source(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url),
      file_system_context_);

  EXPECT_CHECK_DEATH_WITH(source.Signal(base::FilePath()), "can_notify");
}

TEST_F(FileSystemAccessChangeSourceDeathTest,
       NotifyOfUsageChangeBeforeInitialization) {
  auto file_url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal,
      dir_.GetPath().AppendASCII("file"));
  FakeChangeSource source(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url),
      file_system_context_);

  EXPECT_CHECK_DEATH_WITH(source.SignalUsageChange(1, 2), "can_notify");
}

TEST_F(FileSystemAccessChangeSourceDeathTest,
       NotifyOfChangeAfterFailedInitialization) {
  auto file_url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal,
      dir_.GetPath().AppendASCII("file"));
  FakeChangeSource source(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url),
      file_system_context_,
      blink::mojom::FileSystemAccessStatus::kOperationFailed);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  source.EnsureInitialized(future.GetCallback());
  ASSERT_EQ(future.Get()->status,
            blink::mojom::FileSystemAccessStatus::kOperationFailed);

  EXPECT_CHECK_DEATH_WITH(source.Signal(file_url), "can_notify");
  EXPECT_CHECK_DEATH_WITH(source.Signal(base::FilePath()), "can_notify");
  EXPECT_CHECK_DEATH_WITH(source.SignalUsageChange(1, 2), "can_notify");
}
#endif

// A callback passed to `EnsureInitialized` may result in `this` being
// destroyed. This tests that `DidInitialize` (which calls the callbacks) is
// robust to that situation. See https://crbug.com/497880137.
TEST_F(FileSystemAccessChangeSourceTest, TestDestroyFromInitializeCallback) {
  auto file_path = dir_.GetPath().AppendASCII("file");
  auto file_url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal, file_path);

  auto scope = FileSystemAccessWatchScope::GetScopeForFileWatch(file_url);
  FakeChangeSource* source = new FakeChangeSource(scope, file_system_context_);

  source->EnsureInitialized(base::BindOnce(
      [](FakeChangeSource* source, blink::mojom::FileSystemAccessErrorPtr) {
        delete source;
      },
      base::Unretained(source)));
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  source->EnsureInitialized(future.GetCallback());
  EXPECT_EQ(future.Get()->status, blink::mojom::FileSystemAccessStatus::kOk);
}

}  // namespace content
