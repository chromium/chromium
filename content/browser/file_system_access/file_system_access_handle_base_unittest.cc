// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_handle_base.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"
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

using base::test::RunOnceCallback;
using blink::mojom::PermissionStatus;
using storage::FileSystemURL;
using testing::_;
using FileSystemAccessPermissionMode =
    blink::mojom::FileSystemAccessPermissionMode;
using FileSystemAccessStatus = blink::mojom::FileSystemAccessStatus;
using UserActivationState =
    FileSystemAccessPermissionGrant::UserActivationState;
using HandleType = FileSystemAccessPermissionContext::HandleType;

// Verifies that the `FileSystemAccessStatus` and `PermissionStatus` returned by
// `DoRequestPermission()` match the expected values.
//
// `expected_fsa_status`: The expected `FileSystemAccessStatus`.
// `expected_permission_status`: The expected `PermissionStatus`.
MATCHER_P2(PermissionStatusIs,
           expected_fsa_status,
           expected_permission_status,
           base::StrCat({"has FileSystemAccessStatus ",
                         testing::PrintToString(expected_fsa_status),
                         " and PermissionStatus ",
                         testing::PrintToString(expected_permission_status)})) {
  // `arg` is a `const TestFuture<blink::mojom::FileSystemAccessErrorPtr,
  // PermissionStatus>&`. The const_cast is needed to call the non-const
  // `Get()` overload on `TestFuture`, which might be required for some
  // interactions with `mojo::StructPtr` or specific compiler versions.
  auto& mutable_arg =
      const_cast<base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                                        PermissionStatus>&>(arg);
  const auto& fs_status = mutable_arg.template Get<0>()->status;
  const auto& permission_status = mutable_arg.template Get<1>();

  bool fsa_status_ok = (fs_status == expected_fsa_status);
  bool permission_status_ok = (permission_status == expected_permission_status);

  if (fsa_status_ok && permission_status_ok) {
    return true;
  }

  if (!fsa_status_ok) {
    *result_listener << "whose FileSystemAccessStatus is "
                     << testing::PrintToString(fs_status);
  }
  if (!permission_status_ok) {
    if (!fsa_status_ok) {
      *result_listener << " and";
    }
    *result_listener << " whose PermissionStatus is "
                     << testing::PrintToString(permission_status);
  }
  return false;
}

}  // namespace

class TestFileSystemAccessHandle final : public FileSystemAccessHandleBase {
 public:
  using FileSystemAccessHandleBase::GetRenamePermission;
  using FileSystemAccessHandleBase::RenamePermission;

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
  virtual bool is_worker() const = 0;

  // Sets up a mock expectation for a permission request.
  //
  // This method configures the mock `grant` to expect a call to
  // `RequestPermission_`. When the call occurs, it simulates a user action,
  // i.e. `outcome`, and sets the new permission `new_status`. This is only
  // applicable for tests run in a frame context.
  void ExpectRequestPermission(
      MockFileSystemAccessPermissionGrant& grant,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome,
      PermissionStatus new_status) {
    if (is_worker()) {
      return;
    }
    EXPECT_CALL(grant,
                RequestPermission_(kFrameId, UserActivationState::kRequired, _))
        .WillOnce(
            testing::DoAll(testing::InvokeWithoutArgs([&grant, new_status]() {
                             EXPECT_CALL(grant, GetStatus())
                                 .WillRepeatedly(testing::Return(new_status));
                           }),
                           RunOnceCallback<2>(outcome)));
  }

  // Creates a `TestFileSystemAccessHandle` in a frame or worker context with a
  // default file path.
  TestFileSystemAccessHandle CreateTestHandle() {
    auto context = is_worker()
                       ? FileSystemAccessManagerImpl::BindingContext(
                             kTestStorageKey, kTestURL, kWorkerProcessId)
                       : FileSystemAccessManagerImpl::BindingContext(
                             kTestStorageKey, kTestURL, kFrameId);
    return TestFileSystemAccessHandle(
        manager_.get(), context,
        FileSystemURL::CreateForTest(kTestStorageKey,
                                     storage::kFileSystemTypeTest,
                                     base::FilePath::FromUTF8Unsafe("/test")),
        handle_state_);
  }

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
  auto handle = CreateTestHandle();

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

class FileSystemAccessHandleCheckReadAccessTest
    : public FileSystemAccessHandleParamTestBase {};

TEST_P(FileSystemAccessHandleCheckReadAccessTest, AllowedWhenGranted) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_TRUE(handle.CheckReadAccess().has_value());
}

TEST_P(FileSystemAccessHandleCheckReadAccessTest, DeniedWhenNotGranted) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));
  auto result = handle.CheckReadAccess();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, FileSystemAccessStatus::kPermissionDenied);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::DENIED));
  result = handle.CheckReadAccess();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->status, FileSystemAccessStatus::kPermissionDenied);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessHandleCheckReadAccessTest,
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
  auto handle = CreateTestHandle();

  EXPECT_DEATH_IF_SUPPORTED(handle.GetWritePermissionStatus(), "");
}

// Tests the basic functionality of `GetWritePermissionStatus` when the
// `kFileSystemAccessWriteMode` feature is enabled.
TEST_P(FileSystemAccessHandleGetWritePermissionStatusTest,
       FeatureEnabled_DoesReturnStatus) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

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
  auto handle = CreateTestHandle();

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
  auto handle = CreateTestHandle();

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
 protected:
  // Calls `DoRequestPermission` on the given `handle` and returns a future
  // with the result.
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
  DoRequestPermission(TestFileSystemAccessHandle& handle,
                      FileSystemAccessPermissionMode mode) {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                           PermissionStatus>
        future;
    handle.DoRequestPermission(mode, future.GetCallback());
    return future;
  }

  // Verifies that a permission request from a worker context returns `ASK`
  // without showing a prompt.
  void VerifyWorkerPermissionRequest(TestFileSystemAccessHandle& handle,
                                     FileSystemAccessPermissionMode mode) {
    EXPECT_THAT(
        DoRequestPermission(handle, mode),
        PermissionStatusIs(FileSystemAccessStatus::kOk, PermissionStatus::ASK));
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
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kRead),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::GRANTED));
}

// Tests that `DoRequestPermission` returns `DENIED` if read permission is
// denied when requesting read permission.
TEST_P(FileSystemAccessDoRequestReadPermissionTest, Denied) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kRead);
    return;
  }

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kRead),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
}

// Tests that `DoRequestPermission` returns `DENIED` if read permission is
// already denied when requesting read permission.
TEST_P(FileSystemAccessDoRequestReadPermissionTest, AlreadyDenied) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::DENIED));

  // No permission requests should be made.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kRead),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
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
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::GRANTED));
}

// Tests that `DoRequestPermission` requests both read and write permissions
// when neither are `GRANTED`.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, BothAreNotGranted) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kReadWrite);
    return;
  }

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::GRANTED));
}

// Tests that `DoRequestPermission` requests both read & write permission when
// `kFileSystemAccessWriteMode` is enabled, read permission is `ASK` and write
// permission is `GRANTED`.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest,
       WriteModeEnabled_ReadAskWriteGranted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kReadWrite);
    return;
  }

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::GRANTED));
}

// Tests that `DoRequestPermission` requests both read & write permission when
// `kFileSystemAccessWriteMode` is enabled, read permission is `GRANTED` and
// write permission is `ASK`.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest,
       WriteModeEnabled_ReadGrantedWriteAsk) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kReadWrite);
    return;
  }

  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::GRANTED));
}

// Tests that `DoRequestPermission` requests both read & write permission when
// `kFileSystemAccessWriteMode` is enabled, and both permissions are `ASK`.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest,
       WriteModeEnabled_BothAsk_Granted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kReadWrite);
    return;
  }

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::GRANTED));
}

// Tests that `DoRequestPermission` returns `DENIED` if read permission is
// denied when `kFileSystemAccessWriteMode` is enabled, and both permissions
// are `ASK`.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest,
       WriteModeEnabled_BothAsk_ReadDenied) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kReadWrite);
    return;
  }

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  // The implementation may still request write permission even if the read
  // permission is denied.
  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
}

// Tests that `DoRequestPermission` returns `DENIED` if read permission is
// denied when requesting read & write permission.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, ReadDenied) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kReadWrite);
    return;
  }

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  // The implementation may request both permissions.
  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
}

// Tests that `DoRequestPermission` returns `DENIED` if write permission is
// denied when requesting read & write permission.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, WriteDenied) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kReadWrite);
    return;
  }

  // Read permission is already granted, so it should not be requested again.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
}

// Tests that `DoRequestPermission` returns `DENIED` if write permission is
// already denied when requesting both read & write permission.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, WriteAlreadyDenied) {
  auto handle = CreateTestHandle();

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

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
}

// Tests that `DoRequestPermission` returns `DENIED` if read permission is
// already denied when requesting both read & write permission.
TEST_P(FileSystemAccessDoRequestReadWritePermissionTest, ReadAlreadyDenied) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::DENIED));
  // Write grant status shouldn't matter, but setting it to ASK to be explicit.
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  // No permission requests should be made.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
}

// The params for `DoRequestPermission()`-related tests.
struct RequestPermissionOutcomeTestParam {
  // The name of the test.
  const char* test_name;
  // The outcome of the permission request.
  FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome;
  // The expected status of the request.
  FileSystemAccessStatus expected_status;
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

  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kReadWrite);
    return;
  }

  ExpectRequestPermission(*write_grant_, permission_param.outcome,
                          permission_param.expected_permission_status);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kReadWrite),
      PermissionStatusIs(permission_param.expected_status,
                         permission_param.expected_permission_status));
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
                FileSystemAccessStatus::kSecurityError, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "NoUserActivation",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kNoUserActivation,
                FileSystemAccessStatus::kSecurityError, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "BlockedByContentSetting",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kBlockedByContentSetting,
                FileSystemAccessStatus::kOk, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "UserDenied",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kUserDenied,
                FileSystemAccessStatus::kOk, PermissionStatus::DENIED})),
    [](const testing::TestParamInfo<
        std::tuple<TestContextParam, RequestPermissionOutcomeTestParam>>&
           info) {
      return std::string(std::get<0>(info.param).test_name) + "_" +
             std::get<1>(info.param).test_name;
    });

// Testing the behavior of `DoRequestPermission` for write-only permission
// requests.
class FileSystemAccessDoRequestWritePermissionOutcomeTest
    : public FileSystemAccessDoRequestPermissionTestBase,
      public testing::WithParamInterface<
          std::tuple<TestContextParam, RequestPermissionOutcomeTestParam>> {
 public:
  FileSystemAccessDoRequestWritePermissionOutcomeTest()
      : FileSystemAccessDoRequestPermissionTestBase() {
    features_.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  }
  bool is_worker() const override { return std::get<0>(GetParam()).is_worker; }

 private:
  base::test::ScopedFeatureList features_;
};

// Tests that `DoRequestPermission` returns the correct status for a variety of
// outcomes when requesting write-only permission.
TEST_P(FileSystemAccessDoRequestWritePermissionOutcomeTest,
       WritePermissionOutcome) {
  const RequestPermissionOutcomeTestParam& permission_param =
      std::get<1>(GetParam());

  auto handle = CreateTestHandle();

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kWrite);
    return;
  }

  ExpectRequestPermission(*write_grant_, permission_param.outcome,
                          permission_param.expected_permission_status);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kWrite),
      PermissionStatusIs(permission_param.expected_status,
                         permission_param.expected_permission_status));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDoRequestWritePermissionOutcomeTest,
    testing::Combine(
        testing::ValuesIn(kTestContextParams),
        testing::Values(
            RequestPermissionOutcomeTestParam{
                "InvalidFrame",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kInvalidFrame,
                FileSystemAccessStatus::kSecurityError, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "NoUserActivation",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kNoUserActivation,
                FileSystemAccessStatus::kSecurityError, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "BlockedByContentSetting",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kBlockedByContentSetting,
                FileSystemAccessStatus::kOk, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "UserDenied",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kUserDenied,
                FileSystemAccessStatus::kOk, PermissionStatus::DENIED})),
    [](const testing::TestParamInfo<
        std::tuple<TestContextParam, RequestPermissionOutcomeTestParam>>&
           info) {
      return std::string(std::get<0>(info.param).test_name) + "_" +
             std::get<1>(info.param).test_name;
    });

// Testing the behavior of `DoRequestPermission` for read-only permission
// requests.
class FileSystemAccessDoRequestReadOnlyPermissionOutcomeTest
    : public FileSystemAccessDoRequestPermissionTestBase,
      public testing::WithParamInterface<
          std::tuple<TestContextParam, RequestPermissionOutcomeTestParam>> {
 public:
  bool is_worker() const override { return std::get<0>(GetParam()).is_worker; }
};

// Tests that `DoRequestPermission` returns the correct status for a variety of
// outcomes when requesting read-only permission.
TEST_P(FileSystemAccessDoRequestReadOnlyPermissionOutcomeTest,
       ReadPermissionOutcome) {
  const RequestPermissionOutcomeTestParam& permission_param =
      std::get<1>(GetParam());

  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kRead);
    return;
  }

  ExpectRequestPermission(*read_grant_, permission_param.outcome,
                          permission_param.expected_permission_status);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kRead),
      PermissionStatusIs(permission_param.expected_status,
                         permission_param.expected_permission_status));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDoRequestReadOnlyPermissionOutcomeTest,
    testing::Combine(
        testing::ValuesIn(kTestContextParams),
        testing::Values(
            RequestPermissionOutcomeTestParam{
                "InvalidFrame",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kInvalidFrame,
                FileSystemAccessStatus::kSecurityError, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "NoUserActivation",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kNoUserActivation,
                FileSystemAccessStatus::kSecurityError, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "BlockedByContentSetting",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kBlockedByContentSetting,
                FileSystemAccessStatus::kOk, PermissionStatus::ASK},
            RequestPermissionOutcomeTestParam{
                "UserDenied",
                FileSystemAccessPermissionGrant::PermissionRequestOutcome::
                    kUserDenied,
                FileSystemAccessStatus::kOk, PermissionStatus::DENIED})),
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
  auto handle = CreateTestHandle();

  EXPECT_DEATH_IF_SUPPORTED(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kWrite), "");
}

// Tests that `DoRequestPermission` returns `GRANTED` if write permission is
// already `GRANTED`.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, AlreadyGranted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::GRANTED));
}

// Tests that `DoRequestPermission` returns `GRANTED` if write permission is
// granted when requested.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, Granted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kWrite);
    return;
  }

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::GRANTED));
}

// Tests that `DoRequestPermission` returns `DENIED` if write permission is
// denied when requesting write permission.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, Denied) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  if (is_worker()) {
    VerifyWorkerPermissionRequest(handle,
                                  FileSystemAccessPermissionMode::kWrite);
    return;
  }

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
}

// Tests that `DoRequestPermission` returns `DENIED` if write permission is
// already denied when requesting write permission.
TEST_P(FileSystemAccessDoRequestWritePermissionTest, AlreadyDenied) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::DENIED));

  // No permission requests should be made.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_THAT(
      DoRequestPermission(handle, FileSystemAccessPermissionMode::kWrite),
      PermissionStatusIs(FileSystemAccessStatus::kOk,
                         PermissionStatus::DENIED));
}

class FileSystemAccessDoGetPermissionStatusTest
    : public FileSystemAccessHandleParamTestBase {
 public:
  // Calls `DoGetPermissionStatus` on the given `handle` and waits for the
  // result.
  void DoGetPermissionStatus(TestFileSystemAccessHandle& handle,
                             FileSystemAccessPermissionMode mode,
                             base::test::TestFuture<PermissionStatus>& future) {
    handle.DoGetPermissionStatus(mode, future.GetCallback());
  }
};

// Tests `DoGetPermissionStatus` for read-only permission.
TEST_P(FileSystemAccessDoGetPermissionStatusTest, Read) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));

  base::test::TestFuture<PermissionStatus> future;
  DoGetPermissionStatus(handle, FileSystemAccessPermissionMode::kRead, future);
  EXPECT_EQ(future.Get(), PermissionStatus::ASK);
}

// Tests `DoGetPermissionStatus` for read & write permission.
TEST_P(FileSystemAccessDoGetPermissionStatusTest, ReadWrite) {
  auto handle = CreateTestHandle();

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::GRANTED));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillOnce(testing::Return(PermissionStatus::ASK));

  base::test::TestFuture<PermissionStatus> future;
  DoGetPermissionStatus(handle, FileSystemAccessPermissionMode::kReadWrite,
                        future);
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

// Base class for `RunWithPermission()` tests.
class FileSystemAccessHandleRunWithPermissionTestBase
    : public FileSystemAccessHandleParamTestBase {
 protected:
  // Runs the permission test and asserts the expected outcome.
  //
  // This method executes the `RunWithPermission` function for the given
  // `handle` and `mode`, then compares the resulting status with
  // `expected_status`.
  void RunAndVerifyPermissionOutcome(TestFileSystemAccessHandle& handle,
                                     FileSystemAccessPermissionMode mode,
                                     FileSystemAccessStatus expected_status,
                                     const base::Location& location) {
    base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
    RunWithPermission(handle, mode, future);
    EXPECT_EQ(future.Get()->status, expected_status)
        << "Location: " << location.ToString() << "\n";
  }

  // Calls `RunWithPermission()` on the given `handle` and waits for the result.
  void RunWithPermission(
      TestFileSystemAccessHandle& handle,
      FileSystemAccessPermissionMode mode,
      base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr>& future) {
    handle.RunWithPermission(
        mode,
        base::BindOnce(
            [](base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr>&
                   future,
               blink::mojom::FileSystemAccessErrorPtr result) {
              future.SetValue(std::move(result));
            },
            std::ref(future)),
        base::BindOnce(
            [](base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr>&
                   future,
               blink::mojom::FileSystemAccessErrorPtr error,
               blink::mojom::FileSystemAccessErrorPtr result) {
              future.SetValue(std::move(error));
            },
            std::ref(future)),
        file_system_access_error::Ok());
  }

  // Sets up the initial status expectation for a given permission grant.
  void SetUpGrantInitialStatus(MockFileSystemAccessPermissionGrant& grant,
                               PermissionStatus status) {
    EXPECT_CALL(grant, GetStatus()).WillRepeatedly(testing::Return(status));
  }
};

// Tests `RunWithPermission()` with read-only permission.
class FileSystemAccessHandleRunWithReadPermissionTest
    : public FileSystemAccessHandleRunWithPermissionTestBase {
 public:
  FileSystemAccessHandleRunWithReadPermissionTest()
      : FileSystemAccessHandleRunWithPermissionTestBase() {
    features_.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Verifies the callback executes when read permission is already granted.
TEST_P(FileSystemAccessHandleRunWithReadPermissionTest, Granted_Success) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::GRANTED);

  RunAndVerifyPermissionOutcome(handle, FileSystemAccessPermissionMode::kRead,
                                FileSystemAccessStatus::kOk, FROM_HERE);
}

// Verifies the callback executes after requesting and receiving read
// permission.
TEST_P(FileSystemAccessHandleRunWithReadPermissionTest, Ask_Granted_Success) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  RunAndVerifyPermissionOutcome(handle, FileSystemAccessPermissionMode::kRead,
                                is_worker()
                                    ? FileSystemAccessStatus::kPermissionDenied
                                    : FileSystemAccessStatus::kOk,
                                FROM_HERE);
}

// Ensures an error is returned if read permission is denied.
TEST_P(FileSystemAccessHandleRunWithReadPermissionTest, Ask_Denied_Error) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  RunAndVerifyPermissionOutcome(handle, FileSystemAccessPermissionMode::kRead,
                                FileSystemAccessStatus::kPermissionDenied,
                                FROM_HERE);
}

// Ensures an error is returned if read permission was already denied.
TEST_P(FileSystemAccessHandleRunWithReadPermissionTest, Denied_Error) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::DENIED);

  RunAndVerifyPermissionOutcome(handle, FileSystemAccessPermissionMode::kRead,
                                FileSystemAccessStatus::kPermissionDenied,
                                FROM_HERE);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessHandleRunWithReadPermissionTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

// Tests `RunWithPermission()` with write-only permission.
class FileSystemAccessHandleRunWithWritePermissionTest
    : public FileSystemAccessHandleRunWithPermissionTestBase {
 public:
  FileSystemAccessHandleRunWithWritePermissionTest()
      : FileSystemAccessHandleRunWithPermissionTestBase() {
    features_.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Verifies the callback executes when write permission is already granted.
TEST_P(FileSystemAccessHandleRunWithWritePermissionTest, Granted_Success) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::GRANTED);

  RunAndVerifyPermissionOutcome(handle, FileSystemAccessPermissionMode::kWrite,
                                FileSystemAccessStatus::kOk, FROM_HERE);
}

// Verifies the callback executes after requesting and receiving write
// permission.
TEST_P(FileSystemAccessHandleRunWithWritePermissionTest, Ask_Granted_Success) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  RunAndVerifyPermissionOutcome(handle, FileSystemAccessPermissionMode::kWrite,
                                is_worker()
                                    ? FileSystemAccessStatus::kPermissionDenied
                                    : FileSystemAccessStatus::kOk,
                                FROM_HERE);
}

// Ensures an error is returned if write permission is denied.
TEST_P(FileSystemAccessHandleRunWithWritePermissionTest, Ask_Denied_Error) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  RunAndVerifyPermissionOutcome(handle, FileSystemAccessPermissionMode::kWrite,
                                FileSystemAccessStatus::kPermissionDenied,
                                FROM_HERE);
}

// Ensures an error is returned if write permission was already denied.
TEST_P(FileSystemAccessHandleRunWithWritePermissionTest, Denied_Error) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::DENIED);

  RunAndVerifyPermissionOutcome(handle, FileSystemAccessPermissionMode::kWrite,
                                FileSystemAccessStatus::kPermissionDenied,
                                FROM_HERE);
}

// Confirms that the test will fail as expected if the
// `kFileSystemAccessWriteMode` feature is disabled.
TEST_P(FileSystemAccessHandleRunWithWritePermissionTest, FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr> future;
  EXPECT_DEATH_IF_SUPPORTED(
      RunWithPermission(handle, FileSystemAccessPermissionMode::kWrite, future),
      "");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessHandleRunWithWritePermissionTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

// Tests `RunWithPermission()` with read-write permission.
class FileSystemAccessHandleRunWithReadWritePermissionTest
    : public FileSystemAccessHandleRunWithPermissionTestBase {};

// Verifies the callback executes when both read and write permissions are
// already granted.
TEST_P(FileSystemAccessHandleRunWithReadWritePermissionTest,
       AllGranted_Success) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::GRANTED);
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::GRANTED);

  RunAndVerifyPermissionOutcome(handle,
                                FileSystemAccessPermissionMode::kReadWrite,
                                FileSystemAccessStatus::kOk, FROM_HERE);
}

// Verifies the callback executes when read permission is granted, and write
// permission is then requested and granted.
TEST_P(FileSystemAccessHandleRunWithReadWritePermissionTest,
       ReadGranted_WriteAsk_Granted_Success) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::GRANTED);
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  RunAndVerifyPermissionOutcome(
      handle, FileSystemAccessPermissionMode::kReadWrite,
      is_worker() ? FileSystemAccessStatus::kPermissionDenied
                  : FileSystemAccessStatus::kOk,
      FROM_HERE);
}

// Ensures an error is returned if write permission is denied.
TEST_P(FileSystemAccessHandleRunWithReadWritePermissionTest,
       ReadGranted_WriteAsk_Denied_Error) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::GRANTED);
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  RunAndVerifyPermissionOutcome(
      handle, FileSystemAccessPermissionMode::kReadWrite,
      FileSystemAccessStatus::kPermissionDenied, FROM_HERE);
}

// Verifies the callback executes when write permission is already granted, and
// read permission is then requested and granted.
// This is only possible when the write mode feature is enabled as it separates
// the read and write permission status.
TEST_P(FileSystemAccessHandleRunWithReadWritePermissionTest,
       WriteModeEnabled_ReadAsk_Granted_WriteGranted_Success) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::ASK);
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::GRANTED);

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);
  // Expects no permission request for write grant, as write permission status
  // is separated from read permission status.

  RunAndVerifyPermissionOutcome(
      handle, FileSystemAccessPermissionMode::kReadWrite,
      is_worker() ? FileSystemAccessStatus::kPermissionDenied
                  : FileSystemAccessStatus::kOk,
      FROM_HERE);
}

// Ensures an error is returned if read permission is denied while write
// permission is already granted.
// This is only possible when the write mode feature is enabled as it separates
// the read and write permission status.
TEST_P(FileSystemAccessHandleRunWithReadWritePermissionTest,
       WriteModeEnabled_ReadAsk_Denied_WriteGranted_Error) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::ASK);
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::GRANTED);

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);
  // Expects no permission request for write grant, as write permission status
  // is separated from read permission status.

  RunAndVerifyPermissionOutcome(
      handle, FileSystemAccessPermissionMode::kReadWrite,
      FileSystemAccessStatus::kPermissionDenied, FROM_HERE);
}

// Verifies the callback executes when both permissions are requested and
// granted.
TEST_P(FileSystemAccessHandleRunWithReadWritePermissionTest,
       AllAsk_AllGranted_Success) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::ASK);
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);
  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  RunAndVerifyPermissionOutcome(
      handle, FileSystemAccessPermissionMode::kReadWrite,
      is_worker() ? FileSystemAccessStatus::kPermissionDenied
                  : FileSystemAccessStatus::kOk,
      FROM_HERE);
}

// Ensures an error is returned if read permission is denied when both are
// requested.
TEST_P(FileSystemAccessHandleRunWithReadWritePermissionTest,
       AllAsk_ReadDenied_Error) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::ASK);
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);
  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);

  RunAndVerifyPermissionOutcome(
      handle, FileSystemAccessPermissionMode::kReadWrite,
      FileSystemAccessStatus::kPermissionDenied, FROM_HERE);
}

// Ensures an error is returned if write permission is denied when both are
// requested.
TEST_P(FileSystemAccessHandleRunWithReadWritePermissionTest,
       AllAsk_WriteDenied_Error) {
  auto handle = CreateTestHandle();
  SetUpGrantInitialStatus(*read_grant_, PermissionStatus::ASK);
  SetUpGrantInitialStatus(*write_grant_, PermissionStatus::ASK);

  ExpectRequestPermission(
      *read_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserGranted,
      PermissionStatus::GRANTED);
  ExpectRequestPermission(
      *write_grant_,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome::kUserDenied,
      PermissionStatus::DENIED);

  RunAndVerifyPermissionOutcome(
      handle, FileSystemAccessPermissionMode::kReadWrite,
      FileSystemAccessStatus::kPermissionDenied, FROM_HERE);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessHandleRunWithReadWritePermissionTest,
    testing::ValuesIn(kTestContextParams),
    [](const testing::TestParamInfo<TestContextParam>& info) {
      return info.param.test_name;
    });

class FileSystemAccessHandleGetRenamePermissionTest
    : public FileSystemAccessHandleTestBase {
 public:
  void SetUp() override {
    FileSystemAccessHandleTestBase::SetUp();
    manager_->SetPermissionContextForTesting(&permission_context_);

    dummy_read_grant_ = base::MakeRefCounted<
        testing::NiceMock<MockFileSystemAccessPermissionGrant>>();
    ON_CALL(permission_context_, GetReadPermissionGrant(_, _, _, _))
        .WillByDefault(testing::Return(dummy_read_grant_));

    dummy_write_grant_ = base::MakeRefCounted<
        testing::NiceMock<MockFileSystemAccessPermissionGrant>>();
    ON_CALL(*dummy_write_grant_, GetStatus())
        .WillByDefault(testing::Return(PermissionStatus::ASK));
    ON_CALL(permission_context_, GetWritePermissionGrant(_, _, _, _))
        .WillByDefault(testing::Return(dummy_write_grant_));
  }

  void TearDown() override {
    manager_->SetPermissionContextForTesting(nullptr);
    FileSystemAccessHandleTestBase::TearDown();
  }

 protected:
  bool is_worker() const override { return false; }

  void SetUpMockGrants(
      const storage::FileSystemURL& dest_url,
      PermissionStatus target_status,
      std::optional<PermissionStatus> parent_status = std::nullopt) {
    auto target_write_grant = base::MakeRefCounted<
        testing::NiceMock<MockFileSystemAccessPermissionGrant>>();
    ON_CALL(*target_write_grant, GetStatus())
        .WillByDefault(testing::Return(target_status));
    EXPECT_CALL(permission_context_,
                GetWritePermissionGrant(
                    kTestStorageKey.origin(),
                    testing::Field(&content::PathInfo::path, dest_url.path()),
                    HandleType::kFile,
                    FileSystemAccessPermissionContext::AccessTrigger::
                        kProgrammaticRead))
        .WillOnce(testing::Return(target_write_grant));

    if (parent_status.has_value()) {
      auto parent_write_grant = base::MakeRefCounted<
          testing::NiceMock<MockFileSystemAccessPermissionGrant>>();
      ON_CALL(*parent_write_grant, GetStatus())
          .WillByDefault(testing::Return(*parent_status));
      EXPECT_CALL(
          permission_context_,
          GetWritePermissionGrant(kTestStorageKey.origin(),
                                  testing::Field(&content::PathInfo::path,
                                                 dest_url.path().DirName()),
                                  HandleType::kDirectory,
                                  FileSystemAccessPermissionContext::
                                      AccessTrigger::kProgrammaticRead))
          .WillOnce(testing::Return(parent_write_grant));
    } else {
      EXPECT_CALL(permission_context_,
                  GetWritePermissionGrant(_, _, HandleType::kDirectory, _))
          .Times(0);
    }
  }

  testing::NiceMock<MockFileSystemAccessPermissionContext> permission_context_;
  scoped_refptr<MockFileSystemAccessPermissionGrant> dummy_read_grant_;
  scoped_refptr<MockFileSystemAccessPermissionGrant> dummy_write_grant_;
};

TEST_F(FileSystemAccessHandleGetRenamePermissionTest, OPFSDestination) {
  auto handle = CreateTestHandle();
  storage::FileSystemURL dest_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("dest"));

  EXPECT_EQ(handle.GetRenamePermission("dest",
                                       /*has_transient_user_activation=*/false,
                                       dest_url),
            TestFileSystemAccessHandle::RenamePermission::kAllowOverwrite);
}

TEST_F(FileSystemAccessHandleGetRenamePermissionTest,
       TargetFileHasWriteAccess) {
  auto handle = CreateTestHandle();
  storage::FileSystemURL dest_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal,
      base::FilePath::FromUTF8Unsafe("/test/dest"));

  SetUpMockGrants(dest_url, PermissionStatus::GRANTED);

  EXPECT_EQ(handle.GetRenamePermission("dest",
                                       /*has_transient_user_activation=*/false,
                                       dest_url),
            TestFileSystemAccessHandle::RenamePermission::kAllowOverwrite);
}

TEST_F(FileSystemAccessHandleGetRenamePermissionTest, FlagOff_WithGesture) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kFileSystemAccessRenameRequiresParentWritePermission);

  auto handle = CreateTestHandle();
  storage::FileSystemURL dest_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal,
      base::FilePath::FromUTF8Unsafe("/test/dest"));

  SetUpMockGrants(dest_url, PermissionStatus::ASK);

  EXPECT_EQ(handle.GetRenamePermission("dest",
                                       /*has_transient_user_activation=*/true,
                                       dest_url),
            TestFileSystemAccessHandle::RenamePermission::
                kAllowRenameWithoutOverwrite);
}

TEST_F(FileSystemAccessHandleGetRenamePermissionTest, FlagOff_NoGesture) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kFileSystemAccessRenameRequiresParentWritePermission);

  auto handle = CreateTestHandle();
  storage::FileSystemURL dest_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal,
      base::FilePath::FromUTF8Unsafe("/test/dest"));

  SetUpMockGrants(dest_url, PermissionStatus::ASK);

  EXPECT_EQ(handle.GetRenamePermission("dest",
                                       /*has_transient_user_activation=*/false,
                                       dest_url),
            TestFileSystemAccessHandle::RenamePermission::kDeny);
}

TEST_F(FileSystemAccessHandleGetRenamePermissionTest,
       FlagOn_ParentHasWriteAccess) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFileSystemAccessRenameRequiresParentWritePermission,
      {{features::kOnlyInHomedir.name, "false"}});

  auto handle = CreateTestHandle();
  storage::FileSystemURL dest_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal,
      base::FilePath::FromUTF8Unsafe("/test/dest"));

  SetUpMockGrants(dest_url, PermissionStatus::ASK, PermissionStatus::GRANTED);

  EXPECT_EQ(handle.GetRenamePermission("dest",
                                       /*has_transient_user_activation=*/false,
                                       dest_url),
            TestFileSystemAccessHandle::RenamePermission::kAllowOverwrite);
}

TEST_F(FileSystemAccessHandleGetRenamePermissionTest,
       FlagOn_ParentNoWriteAccess) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFileSystemAccessRenameRequiresParentWritePermission,
      {{features::kOnlyInHomedir.name, "false"}});

  auto handle = CreateTestHandle();
  storage::FileSystemURL dest_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal,
      base::FilePath::FromUTF8Unsafe("/test/dest"));

  SetUpMockGrants(dest_url, PermissionStatus::ASK, PermissionStatus::ASK);

  EXPECT_EQ(
      handle.GetRenamePermission(
          "dest",
          /*has_transient_user_activation=*/true,  // gesture shouldn't bypass
          dest_url),
      TestFileSystemAccessHandle::RenamePermission::kDeny);
}

TEST_F(FileSystemAccessHandleGetRenamePermissionTest, OnlyInHomedir_InHomeDir) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFileSystemAccessRenameRequiresParentWritePermission,
      {{features::kOnlyInHomedir.name, "true"}});

  base::FilePath home_dir = dir_.GetPath().AppendASCII("home");
  ASSERT_TRUE(base::CreateDirectory(home_dir));
  home_dir = base::MakeAbsoluteFilePath(home_dir);
  ASSERT_FALSE(home_dir.empty());
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir);

  auto handle = CreateTestHandle();
  // Destination is inside the home directory: /home/test/dest
  storage::FileSystemURL dest_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal,
      home_dir.AppendASCII("test").AppendASCII("dest"));

  SetUpMockGrants(dest_url, PermissionStatus::ASK, PermissionStatus::ASK);

  EXPECT_EQ(handle.GetRenamePermission("dest",
                                       /*has_transient_user_activation=*/true,
                                       dest_url),
            TestFileSystemAccessHandle::RenamePermission::kDeny);
}

TEST_F(FileSystemAccessHandleGetRenamePermissionTest,
       OnlyInHomedir_OutsideHomeDir) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFileSystemAccessRenameRequiresParentWritePermission,
      {{features::kOnlyInHomedir.name, "true"}});

  base::FilePath home_dir = dir_.GetPath().AppendASCII("home");
  ASSERT_TRUE(base::CreateDirectory(home_dir));
  home_dir = base::MakeAbsoluteFilePath(home_dir);
  ASSERT_FALSE(home_dir.empty());
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir);

  auto handle = CreateTestHandle();
  // Destination is outside the home directory: /other/dest
  base::FilePath other_dir = dir_.GetPath().AppendASCII("other");
  ASSERT_TRUE(base::CreateDirectory(other_dir));
  other_dir = base::MakeAbsoluteFilePath(other_dir);
  ASSERT_FALSE(other_dir.empty());
  storage::FileSystemURL dest_url = FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal,
      other_dir.AppendASCII("dest"));

  SetUpMockGrants(dest_url, PermissionStatus::ASK, std::nullopt);

  EXPECT_EQ(handle.GetRenamePermission("dest",
                                       /*has_transient_user_activation=*/true,
                                       dest_url),
            TestFileSystemAccessHandle::RenamePermission::
                kAllowRenameWithoutOverwrite);
}


class FileSystemAccessHandleDoRequestPermissionLifecycleTest
    : public FileSystemAccessDoRequestPermissionTestBase {
 protected:
  bool is_worker() const override { return false; }
};

// Tests that `DoRequestPermission()` does not encounter a UAF when
// `kFileSystemAccessWriteMode` is enabled and the handle is destroyed during
// the first grant's `RequestPermission()` call. Verifies that the second grant
// request is not invoked and the completion callback is dropped without
// executing.
TEST_F(FileSystemAccessHandleDoRequestPermissionLifecycleTest,
       HandleDestroyedDuringFirstGrantRequest_WriteModeEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);

  auto handle = std::make_unique<TestFileSystemAccessHandle>(
      manager_.get(),
      FileSystemAccessManagerImpl::BindingContext(kTestStorageKey, kTestURL,
                                                  kFrameId),
      FileSystemURL::CreateForTest(kTestStorageKey,
                                   storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test")),
      handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  // Simulates the handle being destroyed (e.g. pipe disconnect) during the
  // first grant request.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(kFrameId, UserActivationState::kRequired, _))
      .WillOnce(
          [&](GlobalRenderFrameHostId, UserActivationState,
              base::OnceCallback<void(
                  FileSystemAccessPermissionGrant::PermissionRequestOutcome)>&
                  cb) {
            handle.reset();
            std::move(cb).Run(FileSystemAccessPermissionGrant::
                                  PermissionRequestOutcome::kUserGranted);
          });

  // Because the handle is destroyed, the subsequent write grant request should
  // not be invoked on a dangling handle.
  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  handle->DoRequestPermission(FileSystemAccessPermissionMode::kReadWrite,
                              future.GetCallback());
  EXPECT_FALSE(future.IsReady());
}

// Tests that `DoRequestPermission()` does not encounter a UAF when
// `kFileSystemAccessWriteMode` is disabled and the handle is destroyed during
// the first grant's `RequestPermission()` call. Verifies that the second grant
// request is not invoked and the completion callback is dropped without
// executing.
TEST_F(FileSystemAccessHandleDoRequestPermissionLifecycleTest,
       HandleDestroyedDuringFirstGrantRequest_WriteModeDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(blink::features::kFileSystemAccessWriteMode);

  auto handle = std::make_unique<TestFileSystemAccessHandle>(
      manager_.get(),
      FileSystemAccessManagerImpl::BindingContext(kTestStorageKey, kTestURL,
                                                  kFrameId),
      FileSystemURL::CreateForTest(kTestStorageKey,
                                   storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test")),
      handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  // Simulates the handle being destroyed (e.g. pipe disconnect) during the
  // first grant request.
  EXPECT_CALL(*read_grant_,
              RequestPermission_(kFrameId, UserActivationState::kRequired, _))
      .WillOnce(
          [&](GlobalRenderFrameHostId, UserActivationState,
              base::OnceCallback<void(
                  FileSystemAccessPermissionGrant::PermissionRequestOutcome)>&
                  cb) {
            handle.reset();
            std::move(cb).Run(FileSystemAccessPermissionGrant::
                                  PermissionRequestOutcome::kUserGranted);
          });

  // Because the handle is destroyed, the subsequent write grant request should
  // not be invoked on a dangling handle.
  EXPECT_CALL(*write_grant_,
              RequestPermission_(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  handle->DoRequestPermission(FileSystemAccessPermissionMode::kReadWrite,
                              future.GetCallback());
  EXPECT_FALSE(future.IsReady());
}

// Tests that `DoRequestPermission()` does not encounter a UAF when
// `kFileSystemAccessWriteMode` is enabled and the handle is destroyed during
// the second grant's `RequestPermission()` call. Verifies that the completion
// callback is dropped without executing.
TEST_F(FileSystemAccessHandleDoRequestPermissionLifecycleTest,
       HandleDestroyedDuringSecondGrantRequest_WriteModeEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(blink::features::kFileSystemAccessWriteMode);

  auto handle = std::make_unique<TestFileSystemAccessHandle>(
      manager_.get(),
      FileSystemAccessManagerImpl::BindingContext(kTestStorageKey, kTestURL,
                                                  kFrameId),
      FileSystemURL::CreateForTest(kTestStorageKey,
                                   storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test")),
      handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  EXPECT_CALL(*read_grant_,
              RequestPermission_(kFrameId, UserActivationState::kRequired, _))
      .WillOnce(
          [](GlobalRenderFrameHostId, UserActivationState,
             base::OnceCallback<void(
                 FileSystemAccessPermissionGrant::PermissionRequestOutcome)>&
                 cb) {
            std::move(cb).Run(FileSystemAccessPermissionGrant::
                                  PermissionRequestOutcome::kUserGranted);
          });

  // Simulates the handle being destroyed (e.g. pipe disconnect) during the
  // second grant request.
  EXPECT_CALL(*write_grant_,
              RequestPermission_(kFrameId, UserActivationState::kRequired, _))
      .WillOnce(
          [&](GlobalRenderFrameHostId, UserActivationState,
              base::OnceCallback<void(
                  FileSystemAccessPermissionGrant::PermissionRequestOutcome)>&
                  cb) {
            handle.reset();
            std::move(cb).Run(FileSystemAccessPermissionGrant::
                                  PermissionRequestOutcome::kUserGranted);
          });

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  handle->DoRequestPermission(FileSystemAccessPermissionMode::kReadWrite,
                              future.GetCallback());
  EXPECT_FALSE(future.IsReady());
}

// Tests that `DoRequestPermission()` does not encounter a UAF when
// `kFileSystemAccessWriteMode` is disabled and the handle is destroyed during
// the second grant's `RequestPermission()` call. Verifies that the completion
// callback is dropped without executing.
TEST_F(FileSystemAccessHandleDoRequestPermissionLifecycleTest,
       HandleDestroyedDuringSecondGrantRequest_WriteModeDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(blink::features::kFileSystemAccessWriteMode);

  auto handle = std::make_unique<TestFileSystemAccessHandle>(
      manager_.get(),
      FileSystemAccessManagerImpl::BindingContext(kTestStorageKey, kTestURL,
                                                  kFrameId),
      FileSystemURL::CreateForTest(kTestStorageKey,
                                   storage::kFileSystemTypeTest,
                                   base::FilePath::FromUTF8Unsafe("/test")),
      handle_state_);

  EXPECT_CALL(*read_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));
  EXPECT_CALL(*write_grant_, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  EXPECT_CALL(*read_grant_,
              RequestPermission_(kFrameId, UserActivationState::kRequired, _))
      .WillOnce(
          [](GlobalRenderFrameHostId, UserActivationState,
             base::OnceCallback<void(
                 FileSystemAccessPermissionGrant::PermissionRequestOutcome)>&
                 cb) {
            std::move(cb).Run(FileSystemAccessPermissionGrant::
                                  PermissionRequestOutcome::kUserGranted);
          });

  // Simulates the handle being destroyed (e.g. pipe disconnect) during the
  // second grant request.
  EXPECT_CALL(*write_grant_,
              RequestPermission_(kFrameId, UserActivationState::kRequired, _))
      .WillOnce(
          [&](GlobalRenderFrameHostId, UserActivationState,
              base::OnceCallback<void(
                  FileSystemAccessPermissionGrant::PermissionRequestOutcome)>&
                  cb) {
            handle.reset();
            std::move(cb).Run(FileSystemAccessPermissionGrant::
                                  PermissionRequestOutcome::kUserGranted);
          });

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         PermissionStatus>
      future;
  handle->DoRequestPermission(FileSystemAccessPermissionMode::kReadWrite,
                              future.GetCallback());
  EXPECT_FALSE(future.IsReady());
}

}  // namespace content
