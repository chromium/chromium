// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/file_backed_blob_factory_worker_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
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
constexpr char kId[] = "id";
constexpr char kId2[] = "id2";
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

    mojo::SetDefaultProcessErrorHandler(
        base::BindRepeating(&FileBackedBlobFactoryWorkerImplTest::OnBadMessage,
                            base::Unretained(this)));

    ChildProcessSecurityPolicyImpl::GetInstance()->AddForTesting(process_id_,
                                                                 &context_);
  }

  void TearDown() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(process_id_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void OnBadMessage(const std::string& error) {
    bad_messages_.push_back(error);
  }

  void WaitForBlobCompletion(storage::BlobDataHandle* blob_handle) {
    base::RunLoop loop;
    blob_handle->RunOnConstructionComplete(
        base::IgnoreArgs<storage::BlobStatus>(loop.QuitClosure()));
    loop.Run();
  }

 protected:
  BrowserTaskEnvironment browser_task_environment_{};
  int process_id_ = 3;
  TestBrowserContext context_;
  std::unique_ptr<FileBackedBlobFactoryWorkerImpl> factory_impl_;
  mojo::Remote<blink::mojom::FileBackedBlobFactory> factory_;
  std::vector<std::string> bad_messages_;
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
  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kId, kType,
                         std::move(element));
  base::RunLoop().RunUntilIdle();
  blob.FlushForTesting();

  EXPECT_TRUE(bad_messages_.empty());

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(kId);
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
  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kId, kType,
                         std::move(element));
  base::RunLoop().RunUntilIdle();
  blob.FlushForTesting();

  EXPECT_TRUE(bad_messages_.empty());

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(kType, handle->content_type());
  EXPECT_EQ(kSize, handle->size());
  ASSERT_EQ(storage::BlobStatus::DONE, handle->GetBlobStatus());

  storage::BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendFile(path, kOffset, kSize, base::Time());
  expected_blob_data.set_content_type(kType);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());
}

TEST_F(FileBackedBlobFactoryWorkerImplTest, Register_ExistingUUID) {
  const base::FilePath path = base::FilePath(TEST_PATH("/dir/testfile"));

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(process_id_,
                                                               path);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id_, path));

  auto element1 =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob1;
  factory_->RegisterBlob(blob1.BindNewPipeAndPassReceiver(), kId, kType,
                         std::move(element1));
  base::RunLoop().RunUntilIdle();
  blob1.FlushForTesting();

  EXPECT_TRUE(bad_messages_.empty());

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());

  auto element2 =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob2;
  factory_->RegisterBlob(blob2.BindNewPipeAndPassReceiver(), kId, kType,
                         std::move(element2));
  base::RunLoop().RunUntilIdle();
  blob2.FlushForTesting();

  EXPECT_EQ(bad_messages_.size(), 1u);

  factory_.FlushForTesting();
  EXPECT_FALSE(factory_.is_connected());

  blob2.FlushForTesting();
  EXPECT_FALSE(blob2.is_connected());
}

TEST_F(FileBackedBlobFactoryWorkerImplTest, Register_EmptyUUID) {
  mojo::Remote<blink::mojom::Blob> blob;

  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), "", "",
                         blink::mojom::DataElementFile::New());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, bad_messages_.size());

  factory_.FlushForTesting();
  EXPECT_FALSE(factory_.is_connected());

  blob.FlushForTesting();
  EXPECT_FALSE(blob.is_connected());
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
  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kId, kType,
                         std::move(element));
  base::RunLoop().RunUntilIdle();
  blob.FlushForTesting();

  EXPECT_TRUE(bad_messages_.empty());

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(kType, handle->content_type());
  EXPECT_EQ(kSize, handle->size());
  ASSERT_EQ(storage::BlobStatus::DONE, handle->GetBlobStatus());

  storage::BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendFile(path, kOffset, kSize, base::Time());
  expected_blob_data.set_content_type(kType);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());
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
  factory_->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kId, kType,
                         std::move(element));
  base::RunLoop().RunUntilIdle();
  blob.FlushForTesting();

  auto element2 =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);
  mojo::Remote<blink::mojom::Blob> blob2;
  factory2->RegisterBlob(blob2.BindNewPipeAndPassReceiver(), kId2, kType,
                         std::move(element2));
  base::RunLoop().RunUntilIdle();
  blob2.FlushForTesting();

  EXPECT_TRUE(bad_messages_.empty());

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(&context_)->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());
  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(kType, handle->content_type());
  EXPECT_EQ(kSize, handle->size());
  ASSERT_EQ(storage::BlobStatus::DONE, handle->GetBlobStatus());

  storage::BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendFile(path, kOffset, kSize, base::Time());
  expected_blob_data.set_content_type(kType);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());

  std::unique_ptr<storage::BlobDataHandle> handle2 =
      blob_storage_context->GetBlobDataFromUUID(kId2);
  WaitForBlobCompletion(handle2.get());
  EXPECT_FALSE(handle2->IsBroken());
  EXPECT_EQ(kType, handle2->content_type());
  EXPECT_EQ(kSize, handle2->size());
  ASSERT_EQ(storage::BlobStatus::DONE, handle2->GetBlobStatus());

  storage::BlobDataBuilder expected_blob_data2(kId2);
  expected_blob_data2.AppendFile(path, kOffset, kSize, base::Time());
  expected_blob_data2.set_content_type(kType);
  EXPECT_EQ(expected_blob_data2, *handle2->CreateSnapshot());
}

}  // namespace content
