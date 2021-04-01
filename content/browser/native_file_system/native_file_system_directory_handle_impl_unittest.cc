// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"

#include <iterator>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using storage::FileSystemURL;

class NativeFileSystemDirectoryHandleImplTest : public testing::Test {
 public:
  NativeFileSystemDirectoryHandleImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<NativeFileSystemManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);

    auto url_and_fs = manager_->CreateFileSystemURLFromPath(
        test_src_origin_, dir_.GetPath());

    handle_ = std::make_unique<NativeFileSystemDirectoryHandleImpl>(
        manager_.get(),
        NativeFileSystemManagerImpl::BindingContext(
            test_src_origin_, test_src_url_, /*worker_process_id=*/1),
        url_and_fs.url,
        NativeFileSystemManagerImpl::SharedHandleState(
            allow_grant_, allow_grant_, std::move(url_and_fs.file_system)));
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

 protected:
  const GURL test_src_url_ = GURL("http://example.com/foo");
const url::Origin test_src_origin_ = url::Origin::Create(test_src_url_);

  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<NativeFileSystemManagerImpl> manager_;

  scoped_refptr<FixedNativeFileSystemPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          FixedNativeFileSystemPermissionGrant::PermissionStatus::GRANTED);
  std::unique_ptr<NativeFileSystemDirectoryHandleImpl> handle_;
};

TEST_F(NativeFileSystemDirectoryHandleImplTest, IsSafePathComponent) {
  constexpr const char* kSafePathComponents[] = {
      "a", "a.txt", "a b.txt", "My Computer", ".a", "lnk.zip", "lnk", "a.local",
  };

  constexpr const char* kUnsafePathComponents[] = {
      "",
      ".",
      "..",
      "...",
      "con",
      "con.zip",
      "NUL",
      "NUL.zip",
      "a.",
      "a\"a",
      "a<a",
      "a>a",
      "a?a",
      "a/",
      "a\\",
      "a ",
      "a . .",
      " Computer",
      "My Computer.{a}",
      "My Computer.{20D04FE0-3AEA-1069-A2D8-08002B30309D}",
      "a\\a",
      "a.lnk",
      "a/a",
      "C:\\",
      "C:/",
      "C:",
  };

  for (const char* component : kSafePathComponents) {
    EXPECT_TRUE(
        NativeFileSystemDirectoryHandleImpl::IsSafePathComponent(component))
        << component;
  }
  for (const char* component : kUnsafePathComponents) {
    EXPECT_FALSE(
        NativeFileSystemDirectoryHandleImpl::IsSafePathComponent(component))
        << component;
  }
}

namespace {
class TestNativeFileSystemDirectoryEntriesListener
    : public blink::mojom::NativeFileSystemDirectoryEntriesListener {
 public:
  TestNativeFileSystemDirectoryEntriesListener(
      std::vector<blink::mojom::NativeFileSystemEntryPtr>* entries,
      base::OnceClosure done)
      : entries_(entries), done_(std::move(done)) {}

  void DidReadDirectory(
      blink::mojom::NativeFileSystemErrorPtr result,
      std::vector<blink::mojom::NativeFileSystemEntryPtr> entries,
      bool has_more_entries) override {
    EXPECT_EQ(result->status, blink::mojom::NativeFileSystemStatus::kOk);
    entries_->insert(entries_->end(), std::make_move_iterator(entries.begin()),
                     std::make_move_iterator(entries.end()));
    if (!has_more_entries) {
      std::move(done_).Run();
    }
  }

 private:
  std::vector<blink::mojom::NativeFileSystemEntryPtr>* entries_;
  base::OnceClosure done_;
};
}  // namespace

TEST_F(NativeFileSystemDirectoryHandleImplTest, GetEntries) {
  constexpr const char* kSafeNames[] = {"a", "a.txt", "My Computer", "lnk.txt",
                                        "a.local"};
  constexpr const char* kUnsafeNames[] = {
      "con",  "con.zip", "NUL",   "a.",
      "a\"a", "a . .",   "a.lnk", "My Computer.{a}",
  };
  for (const char* name : kSafeNames) {
    ASSERT_TRUE(base::WriteFile(dir_.GetPath().AppendASCII(name), "data"))
        << name;
  }
  for (const char* name : kUnsafeNames) {
    base::FilePath file_path = dir_.GetPath().AppendASCII(name);
    bool success = base::WriteFile(file_path, "data");
#if !defined(OS_WIN)
    // Some of the unsafe names are not legal file names on Windows. This is
    // okay, and doesn't materially effect the outcome of the test, so just
    // ignore any failures writing these files to disk.
    EXPECT_TRUE(success) << "Failed to create file " << file_path;
#else
    ignore_result(success);
#endif
  }

  std::vector<blink::mojom::NativeFileSystemEntryPtr> entries;
  base::RunLoop loop;
  mojo::PendingRemote<blink::mojom::NativeFileSystemDirectoryEntriesListener>
      listener;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<TestNativeFileSystemDirectoryEntriesListener>(
          &entries, loop.QuitClosure()),
      listener.InitWithNewPipeAndPassReceiver());
  handle_->GetEntries(std::move(listener));
  loop.Run();

  std::vector<std::string> names;
  for (const auto& entry : entries) {
    names.push_back(entry->name);
  }
  EXPECT_THAT(names, testing::UnorderedElementsAreArray(kSafeNames));
}

}  // namespace content
