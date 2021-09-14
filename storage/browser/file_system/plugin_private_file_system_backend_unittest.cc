// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/plugin_private_file_system_backend.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/browser/test/test_file_system_options.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

using url::Origin;

namespace storage {

namespace {

const FileSystemType kType = kFileSystemTypePluginPrivate;

void DidOpenFileSystem(base::File::Error* error_out, base::File::Error error) {
  *error_out = error;
}

}  // namespace

class PluginPrivateFileSystemBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, data_dir_.GetPath());
  }

  // TODO(https://crbug.com/1231162): determine whether EME/CDM/plugin private
  // file system will be partitioned and use the appropriate StorageKey
  FileSystemURL CreateURL(const GURL& root_url, const std::string& relative) {
    FileSystemURL root = context_->CrackURL(
        root_url, blink::StorageKey(url::Origin::Create(root_url)));
    return context_->CreateCrackedFileSystemURL(
        root.storage_key(), root.mount_type(),
        root.virtual_path().AppendASCII(relative));
  }

  PluginPrivateFileSystemBackend* backend() const {
    return context_->plugin_private_backend();
  }

  std::string RegisterFileSystem() {
    return IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
        kType, kRootName, base::FilePath());
  }

  const base::FilePath& base_path() const { return backend()->base_path(); }

  const std::string kPlugin1 = "plugin1";
  const std::string kPlugin2 = "plugin2";
  const std::string kRootName = "pluginprivate";

  base::ScopedTempDir data_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<FileSystemContext> context_;
};

// TODO(kinuko,nhiroki): There are a lot of duplicate code in these tests. Write
// helper functions to simplify the tests.

TEST_F(PluginPrivateFileSystemBackendTest, OpenFileSystemBasic) {
  const Origin kOrigin = Origin::Create(GURL("http://www.example.com"));

  const std::string filesystem_id1 = RegisterFileSystem();
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kOrigin, kType, filesystem_id1, kPlugin1,
                                   OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  // Run this again with FAIL_IF_NONEXISTENT to see if it succeeds.
  const std::string filesystem_id2 = RegisterFileSystem();
  error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kOrigin, kType, filesystem_id2, kPlugin1,
                                   OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const GURL root_url(GetIsolatedFileSystemRootURIString(
      kOrigin.GetURL(), filesystem_id1, kRootName));
  FileSystemURL file = CreateURL(root_url, "foo");
  base::FilePath platform_path;
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(context_.get(), file));
  EXPECT_EQ(base::File::FILE_OK, AsyncFileTestHelper::GetPlatformPath(
                                     context_.get(), file, &platform_path));
  EXPECT_TRUE(base_path().AppendASCII("000").AppendASCII(kPlugin1).IsParent(
      platform_path));
}

TEST_F(PluginPrivateFileSystemBackendTest, PluginIsolation) {
  const Origin kOrigin = Origin::Create(GURL("http://www.example.com"));

  // Open filesystem for kPlugin1 and kPlugin2.
  const std::string filesystem_id1 = RegisterFileSystem();
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kOrigin, kType, filesystem_id1, kPlugin1,
                                   OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const std::string filesystem_id2 = RegisterFileSystem();
  error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kOrigin, kType, filesystem_id2, kPlugin2,
                                   OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  // Create 'foo' in kPlugin1.
  const GURL root_url1(GetIsolatedFileSystemRootURIString(
      kOrigin.GetURL(), filesystem_id1, kRootName));
  FileSystemURL file1 = CreateURL(root_url1, "foo");
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(context_.get(), file1));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      context_.get(), file1, AsyncFileTestHelper::kDontCheckSize));

  // See the same path is not available in kPlugin2.
  const GURL root_url2(GetIsolatedFileSystemRootURIString(
      kOrigin.GetURL(), filesystem_id2, kRootName));
  FileSystemURL file2 = CreateURL(root_url2, "foo");
  EXPECT_FALSE(AsyncFileTestHelper::FileExists(
      context_.get(), file2, AsyncFileTestHelper::kDontCheckSize));
}

TEST_F(PluginPrivateFileSystemBackendTest, OriginIsolation) {
  const Origin kOrigin1 = Origin::Create(GURL("http://www.example.com"));
  const Origin kOrigin2 = Origin::Create(GURL("https://www.example.com"));

  // Open filesystem for kOrigin1 and kOrigin2.
  const std::string filesystem_id1 = RegisterFileSystem();
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kOrigin1, kType, filesystem_id1, kPlugin1,
                                   OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const std::string filesystem_id2 = RegisterFileSystem();
  error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kOrigin2, kType, filesystem_id2, kPlugin1,
                                   OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  // Create 'foo' in kOrigin1.
  const GURL root_url1(GetIsolatedFileSystemRootURIString(
      kOrigin1.GetURL(), filesystem_id1, kRootName));
  FileSystemURL file1 = CreateURL(root_url1, "foo");
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(context_.get(), file1));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      context_.get(), file1, AsyncFileTestHelper::kDontCheckSize));

  // See the same path is not available in kOrigin2.
  const GURL root_url2(GetIsolatedFileSystemRootURIString(
      kOrigin2.GetURL(), filesystem_id2, kRootName));
  FileSystemURL file2 = CreateURL(root_url2, "foo");
  EXPECT_FALSE(AsyncFileTestHelper::FileExists(
      context_.get(), file2, AsyncFileTestHelper::kDontCheckSize));
}

TEST_F(PluginPrivateFileSystemBackendTest, DeleteOriginDirectory) {
  const blink::StorageKey kStorageKey1 =
      blink::StorageKey::CreateFromStringForTesting("http://www.example.com");
  const blink::StorageKey kStorageKey2 =
      blink::StorageKey::CreateFromStringForTesting("https://www.example.com");

  // Open filesystem for kStorageKey1.origin() and kStorageKey2.origin().
  const std::string filesystem_id1 = RegisterFileSystem();
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kStorageKey1.origin(), kType, filesystem_id1,
                                   kPlugin1,
                                   OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  const std::string filesystem_id2 = RegisterFileSystem();
  error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kStorageKey2.origin(), kType, filesystem_id2,
                                   kPlugin1,
                                   OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  // Create 'foo' in kStorageKey1.origin().
  const GURL root_url1(GetIsolatedFileSystemRootURIString(
      kStorageKey1.origin().GetURL(), filesystem_id1, kRootName));
  FileSystemURL file1 = CreateURL(root_url1, "foo");
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(context_.get(), file1));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      context_.get(), file1, AsyncFileTestHelper::kDontCheckSize));

  // Create 'foo' in kStorageKey2.origin().
  const GURL root_url2(GetIsolatedFileSystemRootURIString(
      kStorageKey2.origin().GetURL(), filesystem_id2, kRootName));
  FileSystemURL file2 = CreateURL(root_url2, "foo");
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(context_.get(), file2));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      context_.get(), file2, AsyncFileTestHelper::kDontCheckSize));

  // Delete data for kStorageKey1.origin().
  error = backend()->DeleteStorageKeyDataOnFileTaskRunner(
      context_.get(), nullptr, kStorageKey1, kType);
  EXPECT_EQ(base::File::FILE_OK, error);

  // Confirm 'foo' in kStorageKey1.origin() is deleted.
  EXPECT_FALSE(AsyncFileTestHelper::FileExists(
      context_.get(), file1, AsyncFileTestHelper::kDontCheckSize));

  // Confirm 'foo' in kStorageKey2.origin() is NOT deleted.
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      context_.get(), file2, AsyncFileTestHelper::kDontCheckSize));

  // Re-open filesystem for kStorageKey1.origin().
  const std::string filesystem_id3 = RegisterFileSystem();
  error = base::File::FILE_ERROR_FAILED;
  backend()->OpenPrivateFileSystem(kStorageKey1.origin(), kType, filesystem_id3,
                                   kPlugin1,
                                   OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                   base::BindOnce(&DidOpenFileSystem, &error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(base::File::FILE_OK, error);

  // Re-create 'foo' in kStorageKey1.origin().
  const GURL root_url3(GetIsolatedFileSystemRootURIString(
      kStorageKey1.origin().GetURL(), filesystem_id3, kRootName));
  FileSystemURL file3 = CreateURL(root_url3, "foo");
  EXPECT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::CreateFile(context_.get(), file3));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      context_.get(), file3, AsyncFileTestHelper::kDontCheckSize));

  // Confirm 'foo' in kStorageKey1.origin() is re-created.
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      context_.get(), file3, AsyncFileTestHelper::kDontCheckSize));
}

}  // namespace storage
