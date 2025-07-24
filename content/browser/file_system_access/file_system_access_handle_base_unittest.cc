// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_handle_base.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_grant.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {
namespace {

// The process ID of the frame.
constexpr int kFrameProcessId = 1;
// The process ID of the worker.
constexpr int kWorkerProcessId = 1;
// The frame routing ID.
constexpr int kFrameRoutingId = 2;
// The ID of the frame.
const GlobalRenderFrameHostId kFrameId(kFrameProcessId, kFrameRoutingId);

}  // namespace

using base::test::RunOnceCallback;
using blink::mojom::PermissionStatus;
using storage::FileSystemURL;
using UserActivationState =
    FileSystemAccessPermissionGrant::UserActivationState;

class TestFileSystemAccessHandle : public FileSystemAccessHandleBase {
 public:
  TestFileSystemAccessHandle(FileSystemAccessManagerImpl* manager,
                             const BindingContext& context,
                             const storage::FileSystemURL& url,
                             const SharedHandleState& handle_state)
      : FileSystemAccessHandleBase(manager, context, url, handle_state) {}

 private:
  base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
  base::WeakPtrFactory<TestFileSystemAccessHandle> weak_factory_{this};
};

class FileSystemAccessHandleTestBase : public testing::Test {
 public:
  FileSystemAccessHandleTestBase()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);
  }

 protected:
  const GURL kTestURL = GURL("https://example.com/test");
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/");
  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;

  scoped_refptr<MockFileSystemAccessPermissionGrant> read_grant_ =
      base::MakeRefCounted<
          testing::StrictMock<MockFileSystemAccessPermissionGrant>>();
  scoped_refptr<MockFileSystemAccessPermissionGrant> write_grant_ =
      base::MakeRefCounted<
          testing::StrictMock<MockFileSystemAccessPermissionGrant>>();

  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  FileSystemAccessManagerImpl::SharedHandleState handle_state_ = {read_grant_,
                                                                  write_grant_};

  virtual bool is_worker() const = 0;

  // Creates a `TestFileSystemAccessHandle` in a frame or worker context.
  TestFileSystemAccessHandle CreateHandle(const blink::StorageKey& storage_key,
                                          const GURL& url,
                                          const base::FilePath& path) {
    auto context = is_worker() ? FileSystemAccessManagerImpl::BindingContext(
                                     storage_key, url, kWorkerProcessId)
                               : FileSystemAccessManagerImpl::BindingContext(
                                     storage_key, url, kFrameId);
    return TestFileSystemAccessHandle(
        manager_.get(), context,
        FileSystemURL::CreateForTest(storage_key, storage::kFileSystemTypeTest,
                                     path),
        handle_state_);
  }
};

struct TestContextParam {
  bool is_worker;
  const char* test_name;
};

constexpr TestContextParam kTestContextParams[] = {{false, "Frame"},
                                                   {true, "Worker"}};

class FileSystemAccessHandleParamTestBase
    : public FileSystemAccessHandleTestBase,
      public testing::WithParamInterface<TestContextParam> {
 public:
  bool is_worker() const override { return GetParam().is_worker; }
};

class FileSystemAccessHandleGetReadPermissionStatusTest
    : public FileSystemAccessHandleParamTestBase {};

// Tests the basic functionality of `GetReadPermissionStatus`.
TEST_P(FileSystemAccessHandleGetReadPermissionStatusTest, DoesReturnStatus) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));
  EXPECT_EQ(PermissionStatus::ASK, handle.GetReadPermissionStatus());

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_EQ(PermissionStatus::GRANTED, handle.GetReadPermissionStatus());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessHandleGetReadPermissionStatusTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

class FileSystemAccessHandleGetWritePermissionStatusTest
    : public FileSystemAccessHandleParamTestBase {};

// Tests that `GetWritePermissionStatus` dies if the
// `kFileSystemAccessWriteMode` feature is disabled.
TEST_P(FileSystemAccessHandleGetWritePermissionStatusTest, FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_DEATH_IF_SUPPORTED(handle.GetWritePermissionStatus(), "");
}

// Tests the basic functionality of `GetWritePermissionStatus` when the
// `kFileSystemAccessWriteMode` feature is enabled.
TEST_P(FileSystemAccessHandleGetWritePermissionStatusTest,
       FeatureEnabled_DoesReturnStatus) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  // GetWritePermissionStatus should not be affected by the read permission
  // status.
  EXPECT_CALL(*read_grant_, GetStatus()).Times(0);

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK))
      .WillOnce(testing::Return(PermissionStatus::GRANTED))
      .WillOnce(testing::Return(PermissionStatus::DENIED));

  EXPECT_EQ(PermissionStatus::ASK, handle.GetWritePermissionStatus());
  EXPECT_EQ(PermissionStatus::GRANTED, handle.GetWritePermissionStatus());
  EXPECT_EQ(PermissionStatus::DENIED, handle.GetWritePermissionStatus());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessHandleGetWritePermissionStatusTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

class FileSystemAccessHandleGetReadWritePermissionStatusTest
    : public FileSystemAccessHandleParamTestBase {};

// Tests that `GetReadWritePermissionStatus` returns the read permission status
// if the read permission is not `GRANTED`.
TEST_P(FileSystemAccessHandleGetReadWritePermissionStatusTest,
       ReadPermissionNotGranted) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));
  EXPECT_EQ(PermissionStatus::ASK, handle.GetReadWritePermissionStatus());

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::DENIED));
  EXPECT_EQ(PermissionStatus::DENIED, handle.GetReadWritePermissionStatus());
}

// Tests that `GetReadWritePermissionStatus` returns the write permission
// status if the read permission is `GRANTED`.
TEST_P(FileSystemAccessHandleGetReadWritePermissionStatusTest,
       ReadPermissionAlreadyGranted) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));
  EXPECT_EQ(PermissionStatus::ASK, handle.GetReadWritePermissionStatus());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessHandleGetReadWritePermissionStatusTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

// Common test base to cover `DoRequestPermission()`.
class FileSystemAccessDoRequestPermissionTestBase
    : public FileSystemAccessHandleTestBase {
 public:
  // Calls `DoRequestPermission` on the given `handle` and waits for the result
  // using `future`.
  void DoRequestPermission(
      TestFileSystemAccessHandle& handle,
      blink::mojom::FileSystemAccessPermissionMode mode,
      base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                             PermissionStatus>& future) {
    handle.DoRequestPermission(mode, future.GetCallback());
  }
};

class FileSystemAccessDoRequestReadPermissionTest
    : public FileSystemAccessDoRequestPermissionTestBase,
      public testing::WithParamInterface<TestContextParam> {
 public:
  bool is_worker() const override { return GetParam().is_worker; }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDoRequestReadPermissionTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

// Tests that `DoRequestPermission` returns `GRANTED` if write permission is not
// requested and read permission is already `GRANTED`.
TEST_P(FileSystemAccessDoRequestReadPermissionTest, WritePermissionNotNeeded) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kRead, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::GRANTED);
}

// Tests that `DoRequestPermission` returns `DENIED` if read permission is
// denied when requesting read permission.
TEST_P(FileSystemAccessDoRequestReadPermissionTest, Denied) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    // Workers can't show a permission prompt, so the permission status is
    // expected to remain ASK.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           PermissionStatus>
        future;
    DoRequestPermission(
        handle, blink::mojom::FileSystemAccessPermissionMode::kRead, future);
    EXPECT_EQ(future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_EQ(future.Get<1>(), PermissionStatus::ASK);
    return;
  }

  EXPECT_CALL(
      *read_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&]() {
            EXPECT_CALL(*read_grant_, GetStatus())
                .WillOnce(testing::Return(PermissionStatus::DENIED));
          }),
          RunOnceCallback<2>(FileSystemAccessPermissionGrant::
                                 PermissionRequestOutcome::kUserDenied)));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kRead, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::DENIED);
}

// Tests that `DoRequestPermission` returns `DENIED` if read permission is
// already denied when requesting read permission.
TEST_P(FileSystemAccessDoRequestReadPermissionTest, AlreadyDenied) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::DENIED));

  // No permission requests should be made.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kRead, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::DENIED);
}

class FileSystemAccessDoRequestReadWritePermissionTest
    : public FileSystemAccessDoRequestPermissionTestBase,
      public testing::WithParamInterface<TestContextParam> {
 public:
  bool is_worker() const override { return GetParam().is_worker; }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDoRequestReadWritePermissionTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

// Tests that `DoRequestPermission` requests both read and write permissions
// when both are already `GRANTED`.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest,
       BothAreAlreadyGranted) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::GRANTED);
}

// Tests that `DoRequestPermission` requests both read and write permissions
// when neither are `GRANTED`.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, BothAreNotGranted) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    // Workers can't show a permission prompt, so the permission status is
    // expected to remain ASK.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           PermissionStatus>
        future;
    DoRequestPermission(
        handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
        future);
    EXPECT_EQ(future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_EQ(future.Get<1>(), PermissionStatus::ASK);
    return;
  }

  EXPECT_CALL(
      *read_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&]() {
            EXPECT_CALL(*read_grant_, GetStatus())
                .WillOnce(testing::Return(PermissionStatus::GRANTED));
          }),
          RunOnceCallback<2>(FileSystemAccessPermissionGrant::
                                 PermissionRequestOutcome::kUserGranted)));

  EXPECT_CALL(
      *write_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&]() {
            EXPECT_CALL(*write_grant_, GetStatus())
                .WillOnce(testing::Return(PermissionStatus::GRANTED));
          }),
          RunOnceCallback<2>(FileSystemAccessPermissionGrant::
                                 PermissionRequestOutcome::kUserGranted)));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::GRANTED);
}

// Tests that `DoRequestPermission` returns `DENIED` if read permission is
// denied when requesting read & write permission.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, ReadDenied) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    // Workers can't show a permission prompt, so the permission status is
    // expected to remain ASK.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           PermissionStatus>
        future;
    DoRequestPermission(
        handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
        future);
    EXPECT_EQ(future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_EQ(future.Get<1>(), PermissionStatus::ASK);
    return;
  }

  EXPECT_CALL(
      *read_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&]() {
            EXPECT_CALL(*read_grant_, GetStatus())
                .WillOnce(testing::Return(PermissionStatus::DENIED));
          }),
          RunOnceCallback<2>(FileSystemAccessPermissionGrant::
                                 PermissionRequestOutcome::kUserDenied)));

  // The implementation may request both permissions.
  EXPECT_CALL(
      *write_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(RunOnceCallback<2>(FileSystemAccessPermissionGrant::
                                       PermissionRequestOutcome::kUserGranted));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::DENIED);
}

// Tests that `DoRequestPermission` returns `DENIED` if write permission is
// denied when requesting read & write permission.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, WriteDenied) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    // Workers can't show a permission prompt, so the permission status is
    // expected to remain ASK.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           PermissionStatus>
        future;
    DoRequestPermission(
        handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
        future);
    EXPECT_EQ(future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_EQ(future.Get<1>(), PermissionStatus::ASK);
    return;
  }

  // Read permission is already granted, so it should not be requested again.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(
      *write_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&]() {
            EXPECT_CALL(*write_grant_, GetStatus())
                .WillOnce(testing::Return(PermissionStatus::DENIED));
          }),
          RunOnceCallback<2>(FileSystemAccessPermissionGrant::
                                 PermissionRequestOutcome::kUserDenied)));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::DENIED);
}

// Tests that `DoRequestPermission` returns `DENIED` if write permission is
// already denied when requesting both read & write permission.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, WriteAlreadyDenied) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::DENIED));

  // No permission requests should be made.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::DENIED);
}

// The params for `DoRequestPermission()`-related tests.
struct RequestPermissionOutcomeTestParam {
  // The name of the test.
  const char* test_name;
  // The outcome of the permission request.
  FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome;
  // The expected status of the request.
  blink::mojom::FileSystemAccessStatus expected_status;
  // The expected permission status.
  PermissionStatus expected_permission_status;
};

// Testing the behavior of `DoRequestPermission` for read & write permission
// requests.
class FileSystemAccessDoRequestReadWritePermissionOutcomeTest
    : public FileSystemAccessDoRequestPermissionTestBase,
      public testing::WithParamInterface<
          std::tuple<TestContextParam, RequestPermissionOutcomeTestParam>> {
 public:
  bool is_worker() const override { return std::get<0>(GetParam()).is_worker; }
};

// Tests that `DoRequestPermission` returns the correct status for a variety of
// outcomes when read permission is already 'GRANTED'.
TEST_P(FileSystemAccessDoRequestReadWritePermissionOutcomeTest,
       ReadPermissionAlreadyGranted_WritePermissionOutcome) {
  const RequestPermissionOutcomeTestParam& permission_param =
      std::get<1>(GetParam());

  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    // Workers can't show a permission prompt, so the permission status is
    // expected to remain ASK.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           PermissionStatus>
        future;
    DoRequestPermission(
        handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite,
        future);
    EXPECT_EQ(future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_EQ(future.Get<1>(), PermissionStatus::ASK);
    return;
  }

  EXPECT_CALL(
      *write_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(RunOnceCallback<2>(permission_param.outcome));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite, future);
  EXPECT_EQ(future.Get<0>()->status, permission_param.expected_status);
  EXPECT_EQ(future.Get<1>(), permission_param.expected_permission_status);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDoRequestReadWritePermissionOutcomeTest,
    testing::Combine(
        testing::ValuesIn(kTestContextParams),
        testing::Values(
            RequestPermissionOutcomeTestParam{
                "InvalidFrame",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kInvalidFrame,
                blink::mojom::FileSystemAccessStatus::kSecurityError,
                PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "NoUserActivation",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kNoUserActivation,
                blink::mojom::FileSystemAccessStatus::kSecurityError,
                PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "BlockedByContentSetting",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kBlockedByContentSetting,
                blink::mojom::FileSystemAccessStatus::kOk,
                PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "UserDenied",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kUserDenied,
                blink::mojom::FileSystemAccessStatus::kOk,
                PermissionStatus::ASK})),
    [](const testing::TestParamInfo<
        std::tuple<TestContextParam, RequestPermissionOutcomeTestParam>>&
           info) {
      return std::string(std::get<0>(info.param).test_name) + "_" +
             std::get<1>(info.param).test_name;
    });

class FileSystemAccessDoRequestWritePermissionTest
    : public FileSystemAccessDoRequestPermissionTestBase,
      public testing::WithParamInterface<TestContextParam> {
 public:
  bool is_worker() const override { return GetParam().is_worker; }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDoRequestWritePermissionTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

// Tests that `DoRequestPermission` with `kWrite` dies if the
// `kFileSystemAccessWriteMode` feature is disabled.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  EXPECT_DEATH_IF_SUPPORTED(
      DoRequestPermission(
          handle, blink::mojom::FileSystemAccessPermissionMode::kWrite, future),
      "");
}

// Tests that `DoRequestPermission` returns `GRANTED` if write permission is
// already `GRANTED`.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, AlreadyGranted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::GRANTED);
}

// Tests that `DoRequestPermission` returns `GRANTED` if write permission is
// granted when requested.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, Granted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    // Workers can't show a permission prompt, so the permission status is
    // expected to remain ASK.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           PermissionStatus>
        future;
    DoRequestPermission(
        handle, blink::mojom::FileSystemAccessPermissionMode::kWrite, future);
    EXPECT_EQ(future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_EQ(future.Get<1>(), PermissionStatus::ASK);
    return;
  }

  EXPECT_CALL(
      *write_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&]() {
            EXPECT_CALL(*write_grant_, GetStatus())
                .WillOnce(testing::Return(PermissionStatus::GRANTED));
          }),
          RunOnceCallback<2>(FileSystemAccessPermissionGrant::
                                 PermissionRequestOutcome::kUserGranted)));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::GRANTED);
}

// Tests that `DoRequestPermission` returns `DENIED` if write permission is
// denied when requesting write permission.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, Denied) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    // Workers can't show a permission prompt, so the permission status is
    // expected to remain ASK.
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           PermissionStatus>
        future;
    DoRequestPermission(
        handle, blink::mojom::FileSystemAccessPermissionMode::kWrite, future);
    EXPECT_EQ(future.Get<0>()->status,
              blink::mojom::FileSystemAccessStatus::kOk);
    EXPECT_EQ(future.Get<1>(), PermissionStatus::ASK);
    return;
  }

  EXPECT_CALL(
      *write_grant_,
      RequestPermission_(kFrameId, UserActivationState::kRequired, testing::_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&]() {
            EXPECT_CALL(*write_grant_, GetStatus())
                .WillOnce(testing::Return(PermissionStatus::DENIED));
          }),
          RunOnceCallback<2>(FileSystemAccessPermissionGrant::
                                 PermissionRequestOutcome::kUserDenied)));

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::DENIED);
}

// Tests that `DoRequestPermission` returns `DENIED` if write permission is
// already denied when requesting write permission.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, AlreadyDenied) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::DENIED));

  // No permission requests should be made.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  DoRequestPermission(
      handle, blink::mojom::FileSystemAccessPermissionMode::kWrite, future);
  EXPECT_EQ(future.Get<0>()->status, blink::mojom::FileSystemAccessStatus::kOk);
  EXPECT_EQ(future.Get<1>(), PermissionStatus::DENIED);
}

class FileSystemAccessDoGetPermissionStatusTest
    : public FileSystemAccessHandleParamTestBase {
 public:
  // Calls `DoGetPermissionStatus` on the given `handle` and waits for the
  // result.
  void DoGetPermissionStatus(TestFileSystemAccessHandle& handle,
                             blink::mojom::FileSystemAccessPermissionMode mode,
                             base::test::TestFuture<PermissionStatus>& future) {
    handle.DoGetPermissionStatus(mode, future.GetCallback());
  }
};

// Tests `DoGetPermissionStatus` for read-only permission.
TEST_P(FileSystemAccessDoGetPermissionStatusTest, Read) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));

  base::test::TestFuture<PermissionStatus> future;
  DoGetPermissionStatus(
      handle, blink::mojom::FileSystemAccessPermissionMode::kRead, future);
  EXPECT_EQ(future.Get(), PermissionStatus::ASK);
}

// Tests `DoGetPermissionStatus` for read & write permission.
TEST_P(FileSystemAccessDoGetPermissionStatusTest, ReadWrite) {
  auto handle = CreateHandle(kTestStorageKey, kTestURL,
                             base::FilePath::FromUTF8Unsafe("/test"));

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));

  base::test::TestFuture<PermissionStatus> future;
  DoGetPermissionStatus(
      handle, blink::mojom::FileSystemAccessPermissionMode::kReadWrite, future);
  EXPECT_EQ(future.Get(), PermissionStatus::ASK);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDoGetPermissionStatusTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

class FileSystemAccessHandleGetParentURLTest
    : public FileSystemAccessHandleParamTestBase {};

// Tests that `GetParentURL` correctly propagates a custom bucket locator.
TEST_P(FileSystemAccessHandleGetParentURLTest, CustomBucketLocator) {
  auto default_bucket_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("/test"));
  TestFileSystemAccessHandle default_handle(
      manager_.get(),
      FileSystemAccessManagerImpl::BindingContext(kTestStorageKey, kTestURL,
                                                  /*worker_process_id=*/1),
      default_bucket_url, handle_state_);
  EXPECT_FALSE(default_handle.GetParentURLForTesting().bucket());

  auto custom_bucket_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("/test"));
  const auto custom_bucket = storage::BucketLocator(
      storage::BucketId(1),
      blink::StorageKey::CreateFromStringForTesting("http://example/"),
      /*is_default=*/false);
  custom_bucket_url.SetBucket(custom_bucket);
  TestFileSystemAccessHandle custom_handle(
      manager_.get(),
      FileSystemAccessManagerImpl::BindingContext(kTestStorageKey, kTestURL,
                                                  /*worker_process_id=*/1),
      custom_bucket_url, handle_state_);
  EXPECT_TRUE(custom_handle.GetParentURLForTesting().bucket());
  EXPECT_EQ(custom_handle.GetParentURLForTesting().bucket().value(),
            custom_bucket_url.bucket().value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessHandleGetParentURLTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

}  // namespace content
