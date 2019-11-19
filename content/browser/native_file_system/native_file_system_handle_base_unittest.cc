// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_handle_base.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/browser/native_file_system/mock_native_file_system_permission_grant.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

using base::test::RunOnceCallback;
using blink::mojom::PermissionStatus;
using storage::FileSystemURL;

class TestNativeFileSystemHandle : public NativeFileSystemHandleBase {
 public:
  TestNativeFileSystemHandle(NativeFileSystemManagerImpl* manager,
                             const BindingContext& context,
                             const storage::FileSystemURL& url,
                             const SharedHandleState& handle_state)
      : NativeFileSystemHandleBase(manager,
                                   context,
                                   url,
                                   handle_state,
                                   /*is_directory=*/false) {}

 private:
  base::WeakPtr<NativeFileSystemHandleBase> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
  base::WeakPtrFactory<TestNativeFileSystemHandle> weak_factory_{this};
};

class NativeFileSystemHandleBaseTest : public testing::Test {
 public:
  NativeFileSystemHandleBaseTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);
  }

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<NativeFileSystemManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);
  }

 protected:
  const GURL kTestURL = GURL("https://example.com/test");
  const url::Origin kTestOrigin = url::Origin::Create(kTestURL);
  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;

  scoped_refptr<MockNativeFileSystemPermissionGrant> read_grant_ =
      base::MakeRefCounted<
          testing::StrictMock<MockNativeFileSystemPermissionGrant>>();
  scoped_refptr<MockNativeFileSystemPermissionGrant> write_grant_ =
      base::MakeRefCounted<
          testing::StrictMock<MockNativeFileSystemPermissionGrant>>();

  scoped_refptr<NativeFileSystemManagerImpl> manager_;

  NativeFileSystemManagerImpl::SharedHandleState handle_state_ = {read_grant_,
                                                                  write_grant_,
                                                                  {}};
};

TEST_F(NativeFileSystemHandleBaseTest, GetReadPermissionStatus) {
  auto url =
      FileSystemURL::CreateForTest(kTestOrigin, storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test"));
  TestNativeFileSystemHandle handle(manager_.get(),
                                    NativeFileSystemManagerImpl::BindingContext(
                                        kTestOrigin, kTestURL, /*process_id=*/1,
                                        /*frame_id=*/MSG_ROUTING_NONE),
                                    url, handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));
  EXPECT_EQ(PermissionStatus::ASK, handle.GetReadPermissionStatus());

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_EQ(PermissionStatus::GRANTED, handle.GetReadPermissionStatus());
}

TEST_F(NativeFileSystemHandleBaseTest,
       GetWritePermissionStatus_ReadStatusNotGranted) {
  auto url =
      FileSystemURL::CreateForTest(kTestOrigin, storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test"));
  TestNativeFileSystemHandle handle(manager_.get(),
                                    NativeFileSystemManagerImpl::BindingContext(
                                        kTestOrigin, kTestURL, /*process_id=*/1,
                                        /*frame_id=*/MSG_ROUTING_NONE),
                                    url, handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));
  EXPECT_EQ(PermissionStatus::ASK, handle.GetWritePermissionStatus());

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::DENIED));
  EXPECT_EQ(PermissionStatus::DENIED, handle.GetWritePermissionStatus());
}

TEST_F(NativeFileSystemHandleBaseTest,
       GetWritePermissionStatus_ReadStatusGranted) {
  auto url =
      FileSystemURL::CreateForTest(kTestOrigin, storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test"));
  TestNativeFileSystemHandle handle(manager_.get(),
                                    NativeFileSystemManagerImpl::BindingContext(
                                        kTestOrigin, kTestURL, /*process_id=*/1,
                                        /*frame_id=*/MSG_ROUTING_NONE),
                                    url, handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));
  EXPECT_EQ(PermissionStatus::ASK, handle.GetWritePermissionStatus());
}

TEST_F(NativeFileSystemHandleBaseTest, RequestWritePermission_AlreadyGranted) {
  auto url =
      FileSystemURL::CreateForTest(kTestOrigin, storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test"));
  TestNativeFileSystemHandle handle(manager_.get(),
                                    NativeFileSystemManagerImpl::BindingContext(
                                        kTestOrigin, kTestURL, /*process_id=*/1,
                                        /*frame_id=*/MSG_ROUTING_NONE),
                                    url, handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));

  base::RunLoop loop;
  handle.DoRequestPermission(
      /*writable=*/true,
      base::BindLambdaForTesting(
          [&](blink::mojom::NativeFileSystemErrorPtr error,
              PermissionStatus result) {
            EXPECT_EQ(blink::mojom::NativeFileSystemStatus::kOk, error->status);
            EXPECT_EQ(PermissionStatus::GRANTED, result);
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(NativeFileSystemHandleBaseTest, RequestWritePermission) {
  const int kProcessId = 1;
  const int kFrameId = 2;

  auto url =
      FileSystemURL::CreateForTest(kTestOrigin, storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test"));
  TestNativeFileSystemHandle handle(
      manager_.get(),
      NativeFileSystemManagerImpl::BindingContext(kTestOrigin, kTestURL,
                                                  kProcessId, kFrameId),
      url, handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  {
    testing::InSequence sequence;
    EXPECT_CALL(*write_grant_, GetStatus())
        .WillOnce(testing::Return(PermissionStatus::ASK));
    EXPECT_CALL(*write_grant_,
                RequestPermission_(kProcessId, kFrameId, testing::_))
        .WillOnce(
            RunOnceCallback<2>(NativeFileSystemPermissionGrant::
                                   PermissionRequestOutcome::kUserGranted));
    EXPECT_CALL(*write_grant_, GetStatus())
        .WillOnce(testing::Return(PermissionStatus::GRANTED));
  }

  base::RunLoop loop;
  handle.DoRequestPermission(
      /*writable=*/true,
      base::BindLambdaForTesting(
          [&](blink::mojom::NativeFileSystemErrorPtr error,
              PermissionStatus result) {
            EXPECT_EQ(blink::mojom::NativeFileSystemStatus::kOk, error->status);
            EXPECT_EQ(PermissionStatus::GRANTED, result);
            loop.Quit();
          }));
  loop.Run();
}

}  // namespace content
