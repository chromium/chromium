// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/file_backed_blob_factory_worker_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom.h"
#include "third_party/blink/public/mojom/blob/file_backed_blob_factory.mojom.h"
#include "url/gurl.h"

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define TEST_PATH(x) (FILE_PATH_LITERAL("c:") FILE_PATH_LITERAL(x))
#else
#define TEST_PATH(x) (FILE_PATH_LITERAL(x))
#endif

namespace content {
namespace {
constexpr char kType[] = "content/type";
constexpr uint64_t kOffset = 0;
constexpr uint64_t kSize = 16;
constexpr char kUrl[] = "https://example.com";
constexpr char kUrl2[] = "https://2.example.com";
}  // namespace

class FileBackedBlobFactoryWorkerImplTest : public testing::Test {
 public:
  void SetUp() override {
    factory_impl_ = std::make_unique<FileBackedBlobFactoryWorkerImpl>(
        &context_, process_id_);
    factory_impl_->BindReceiver(factory_.BindNewPipeAndPassReceiver(),
                                GURL(kUrl));

    ChildProcessSecurityPolicyImpl::GetInstance()->AddForTesting(process_id_,
                                                                 &context_);
  }

  void TearDown() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(process_id_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void WaitForBlobCompletion(storage::BlobDataHandle* blob_handle) {
    base::RunLoop loop;
    blob_handle->RunOnConstructionComplete(
        base::IgnoreArgs<storage::BlobStatus>(loop.QuitClosure()));
    loop.Run();
  }

 protected:
  BrowserTaskEnvironment browser_task_environment_{};
  const ChildProcessId process_id_{3};
  TestBrowserContext context_;
  std::unique_ptr<FileBackedBlobFactoryWorkerImpl> factory_impl_;
  mojo::Remote<blink::mojom::FileBackedBlobFactory> factory_;
};

TEST_F(FileBackedBlobFactoryWorkerImplTest, Register_UnreadableFile) {
  const base::FilePath path = base::FilePath(TEST_PATH("/dir/testfile"));

  ChildProcessSecurityPolicyImpl::GetInstance()->RevokeAllPermissionsForFile(
      process_id_, path);
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path));

  auto element =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob;
  base::test::TestFuture<std::string> uuid_future;
  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kType,
                         std::move(element),
                         /*block_on_registration=*/false,
                         uuid_future.GetCallback<const std::string&>());
  blob.FlushForTesting();

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(uuid_future.Get());
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(storage::BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE,
            handle->GetBlobStatus());
}

TEST_F(FileBackedBlobFactoryWorkerImplTest, Register_ValidFile) {
  const base::FilePath path = base::FilePath(TEST_PATH("/dir/testfile"));

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(process_id_,
                                                               path);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path));

  auto element =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob;
  base::test::TestFuture<std::string> uuid_future;
  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kType,
                         std::move(element),
                         /*block_on_registration=*/false,
                         uuid_future.GetCallback<const std::string&>());
  blob.FlushForTesting();
  const std::string uuid = uuid_future.Get();

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(uuid);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(kType, handle->content_type());
  EXPECT_EQ(kSize, handle->size());
  ASSERT_EQ(storage::BlobStatus::DONE, handle->GetBlobStatus());

  storage::BlobDataBuilder expected_blob_data(uuid);
  expected_blob_data.AppendFile(path, kOffset, kSize, base::Time());
  expected_blob_data.set_content_type(kType);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());
}

TEST_F(FileBackedBlobFactoryWorkerImplTest,
       Register_ExistingScopedFileAccessDelegate) {
  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate,
              CreateFileAccessCallback(GURL(kUrl)));

  const base::FilePath path = base::FilePath(TEST_PATH("/dir/testfile"));

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(process_id_,
                                                               path);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path));

  auto element =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob;
  base::test::TestFuture<std::string> uuid_future;
  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kType,
                         std::move(element),
                         /*block_on_registration=*/false,
                         uuid_future.GetCallback<const std::string&>());
  blob.FlushForTesting();
  const std::string uuid = uuid_future.Get();

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(uuid);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(kType, handle->content_type());
  EXPECT_EQ(kSize, handle->size());
  ASSERT_EQ(storage::BlobStatus::DONE, handle->GetBlobStatus());

  storage::BlobDataBuilder expected_blob_data(uuid);
  expected_blob_data.AppendFile(path, kOffset, kSize, base::Time());
  expected_blob_data.set_content_type(kType);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());
}

TEST_F(FileBackedBlobFactoryWorkerImplTest,
       Register_InvalidUrlWithScopedFileAccessDelegate) {
  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate, CreateFileAccessCallback).Times(0);

  // Model a worker bound for an opaque origin: the URL passed to
  // BindReceiver() is invalid.
  mojo::Remote<blink::mojom::FileBackedBlobFactory> factory;
  factory_impl_->BindReceiver(factory.BindNewPipeAndPassReceiver(), GURL());

  const base::FilePath path = base::FilePath(TEST_PATH("/dir/testfile"));

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(process_id_,
                                                               path);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path));

  auto element =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob;
  base::test::TestFuture<std::string> uuid_future;
  factory->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kType,
                        std::move(element),
                        /*block_on_registration=*/false,
                        uuid_future.GetCallback<const std::string&>());
  factory.FlushForTesting();
  blob.FlushForTesting();
  const std::string uuid = uuid_future.Get();

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(uuid);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(storage::BlobStatus::DONE, handle->GetBlobStatus());

  // Because the destination URL is unknown, the registered file item must
  // carry an explicit access callback that denies access rather than leaving
  // the decision to the default access path.
  std::unique_ptr<storage::BlobDataSnapshot> snapshot =
      handle->CreateSnapshot();
  EXPECT_EQ(1u, snapshot->items().size());
  if (snapshot->items().size() != 1u) {
    return;
  }
  auto file_access = snapshot->items()[0]->file_access();
  EXPECT_FALSE(file_access.is_null());
  if (file_access.is_null()) {
    return;
  }

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  file_access.Run({path}, future.GetCallback());
  EXPECT_FALSE(future.Take().is_allowed());
}

TEST_F(FileBackedBlobFactoryWorkerImplTest, MultipleBindings) {
  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate,
              CreateFileAccessCallback(GURL(kUrl)));
  EXPECT_CALL(scoped_file_access_delegate,
              CreateFileAccessCallback(GURL(kUrl2)));
  mojo::Remote<blink::mojom::FileBackedBlobFactory> factory2;
  factory_impl_->BindReceiver(factory2.BindNewPipeAndPassReceiver(),
                              GURL(kUrl2));

  const base::FilePath path = base::FilePath(TEST_PATH("/dir/testfile"));

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(process_id_,
                                                               path);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path));

  auto element =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob;
  base::test::TestFuture<std::string> uuid_future;
  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kType,
                         std::move(element),
                         /*block_on_registration=*/false,
                         uuid_future.GetCallback<const std::string&>());
  blob.FlushForTesting();

  auto element2 =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);
  mojo::Remote<blink::mojom::Blob> blob2;
  base::test::TestFuture<std::string> uuid_future2;
  factory2->RegisterBlob(blob2.BindNewPipeAndPassReceiver(), kType,
                         std::move(element2),
                         /*block_on_registration=*/false,
                         uuid_future2.GetCallback<const std::string&>());
  blob2.FlushForTesting();
  const std::string uuid = uuid_future.Get();
  const std::string uuid2 = uuid_future2.Get();

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(uuid);
  WaitForBlobCompletion(handle.get());
  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(kType, handle->content_type());
  EXPECT_EQ(kSize, handle->size());
  ASSERT_EQ(storage::BlobStatus::DONE, handle->GetBlobStatus());

  storage::BlobDataBuilder expected_blob_data(uuid);
  expected_blob_data.AppendFile(path, kOffset, kSize, base::Time());
  expected_blob_data.set_content_type(kType);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());

  std::unique_ptr<storage::BlobDataHandle> handle2 =
      blob_storage_context->GetBlobDataFromUUID(uuid2);
  WaitForBlobCompletion(handle2.get());
  EXPECT_FALSE(handle2->IsBroken());
  EXPECT_EQ(kType, handle2->content_type());
  EXPECT_EQ(kSize, handle2->size());
  ASSERT_EQ(storage::BlobStatus::DONE, handle2->GetBlobStatus());

  storage::BlobDataBuilder expected_blob_data2(uuid2);
  expected_blob_data2.AppendFile(path, kOffset, kSize, base::Time());
  expected_blob_data2.set_content_type(kType);
  EXPECT_EQ(expected_blob_data2, *handle2->CreateSnapshot());
}

}  // namespace content
