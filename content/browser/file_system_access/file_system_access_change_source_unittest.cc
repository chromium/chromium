// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_change_source.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
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
      scoped_refptr<storage::FileSystemContext> file_system_context)
      : FileSystemAccessChangeSource(std::move(scope),
                                     std::move(file_system_context)) {}
  ~FakeChangeSource() override = default;

  // FileSystemAccessChangeSource:
  void Initialize(
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
          on_source_initialized) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_source_initialized),
                                  blink::mojom::FileSystemAccessError::New(
                                      blink::mojom::FileSystemAccessStatus::kOk,
                                      base::File::FILE_OK, "")));
  }

  void Signal(const storage::FileSystemURL& changed_url,
              bool error = false,
              ChangeInfo change_info = ChangeInfo()) {
    NotifyOfChange(changed_url, error, change_info);
  }
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

  MockRawChangeObserver observer;
  source.AddObserver(&observer);

  EXPECT_CALL(observer, OnRawChange(testing::Eq(file_url), testing::IsFalse(),
                                    testing::_, testing::Eq(scope)));
  source.Signal(file_url);

  source.RemoveObserver(&observer);
}

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
