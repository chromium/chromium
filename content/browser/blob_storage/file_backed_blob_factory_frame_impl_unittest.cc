// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/file_backed_blob_factory_frame_impl.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/system/functions.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
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
constexpr char kType[] = "content/type";
constexpr uint64_t kOffset = 0;
constexpr uint64_t kSize = 16;
constexpr char kMainFrameUrl[] = "https://google.com";
constexpr char kSubframeUrl[] = "https://example.com/subframe";
}  // namespace

class FileBackedBlobFactoryFrameImplTest
    : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    process_id_ = main_test_rfh()->GetProcess()->GetID();
    FileBackedBlobFactoryFrameImpl::CreateForCurrentDocument(
        main_test_rfh(), factory_.BindNewEndpointAndPassDedicatedReceiver());

    main_test_rfh()->SetLastCommittedUrl(GURL(kMainFrameUrl));

    mojo::SetDefaultProcessErrorHandler(
        base::BindRepeating(&FileBackedBlobFactoryFrameImplTest::OnBadMessage,
                            base::Unretained(this)));
  }
  void TearDown() override {
    // Clean up error handler, to avoid causing other tests run in the same
    // process from crashing.
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
    RenderViewHostImplTestHarness::TearDown();
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
  ChildProcessId process_id_;
  mojo::AssociatedRemote<blink::mojom::FileBackedBlobFactory> factory_;
  std::vector<std::string> bad_messages_;
};

TEST_F(FileBackedBlobFactoryFrameImplTest, Register_UnreadableFile) {
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
      ChromeBlobStorageContext::GetFor(main_test_rfh()->GetBrowserContext())
          ->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(storage::BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE,
            handle->GetBlobStatus());
}

TEST_F(FileBackedBlobFactoryFrameImplTest, Register_ValidFile) {
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
      ChromeBlobStorageContext::GetFor(main_test_rfh()->GetBrowserContext())
          ->context();

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

TEST_F(FileBackedBlobFactoryFrameImplTest, Register_ExistingUUID) {
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
      ChromeBlobStorageContext::GetFor(main_test_rfh()->GetBrowserContext())
          ->context();

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

TEST_F(FileBackedBlobFactoryFrameImplTest, Register_EmptyUUID) {
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

TEST_F(FileBackedBlobFactoryFrameImplTest,
       Register_ExistingScopedFileAccessDelegate) {
  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate, CreateFileAccessCallback)
      .WillOnce(::testing::Return(base::BindRepeating(
          [](const std::vector<base::FilePath>& files,
             base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
            std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
          })));

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
      ChromeBlobStorageContext::GetFor(main_test_rfh()->GetBrowserContext())
          ->context();

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

TEST_F(FileBackedBlobFactoryFrameImplTest,
       Register_SubframeUsesOwnUrlAsFileAccessDestination) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();
  TestRenderFrameHost* child_rfh = main_test_rfh()->AppendChild("child");
  ASSERT_TRUE(child_rfh);
  child_rfh->SetLastCommittedUrl(GURL(kSubframeUrl));

  mojo::AssociatedRemote<blink::mojom::FileBackedBlobFactory> child_factory;
  FileBackedBlobFactoryFrameImpl::CreateForCurrentDocument(
      child_rfh, child_factory.BindNewEndpointAndPassDedicatedReceiver());

  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  GURL captured_destination;
  EXPECT_CALL(scoped_file_access_delegate, CreateFileAccessCallback)
      .WillOnce(::testing::DoAll(
          ::testing::SaveArg<0>(&captured_destination),
          ::testing::Return(base::BindRepeating(
              [](const std::vector<base::FilePath>& files,
                 base::OnceCallback<void(file_access::ScopedFileAccess)>
                     callback) {
                std::move(callback).Run(
                    file_access::ScopedFileAccess::Allowed());
              }))));

  const base::FilePath path = base::FilePath(TEST_PATH("/dir/testfile"));
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      child_rfh->GetProcess()->GetID(), path);

  auto element =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob;
  child_factory->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kId, kType,
                              std::move(element));
  base::RunLoop().RunUntilIdle();
  blob.FlushForTesting();

  EXPECT_TRUE(bad_messages_.empty());
  EXPECT_EQ(GURL(kSubframeUrl), captured_destination);
  EXPECT_NE(GURL(kMainFrameUrl), captured_destination);
}

TEST_F(FileBackedBlobFactoryFrameImplTest,
       Register_FencedFrameWithScopedFileAccessDelegate) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();
  TestRenderFrameHost* fenced_rfh = main_test_rfh()->AppendFencedFrame();
  EXPECT_TRUE(fenced_rfh);
  if (!fenced_rfh) {
    return;
  }
  fenced_rfh->SetLastCommittedUrl(GURL(kSubframeUrl));

  mojo::AssociatedRemote<blink::mojom::FileBackedBlobFactory> fenced_factory;
  FileBackedBlobFactoryFrameImpl::CreateForCurrentDocument(
      fenced_rfh, fenced_factory.BindNewEndpointAndPassDedicatedReceiver());

  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate, CreateFileAccessCallback).Times(0);

  const base::FilePath path = base::FilePath(TEST_PATH("/dir/testfile"));
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      fenced_rfh->GetProcess()->GetID(), path);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      fenced_rfh->GetProcess()->GetID(), path));

  auto element =
      blink::mojom::DataElementFile::New(path, kOffset, kSize, std::nullopt);

  mojo::Remote<blink::mojom::Blob> blob;
  fenced_factory->RegisterBlob(blob.BindNewPipeAndPassReceiver(), kId, kType,
                               std::move(element));
  fenced_factory.FlushForTesting();
  blob.FlushForTesting();

  EXPECT_TRUE(bad_messages_.empty());

  auto* blob_storage_context =
      ChromeBlobStorageContext::GetFor(main_test_rfh()->GetBrowserContext())
          ->context();

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_storage_context->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(storage::BlobStatus::DONE, handle->GetBlobStatus());

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

}  // namespace content
