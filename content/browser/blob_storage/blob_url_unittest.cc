// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <limits>
#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "storage/browser/blob/blob_url_store_impl.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/fake_blob_data_handle.h"
#include "storage/browser/test/mock_blob_registry_delegate.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

using storage::BlobDataBuilder;
using storage::BlobDataSnapshot;

namespace content {

namespace {

const int kBufferSize = 1024;
const char kTestData1[] = "Hello";
const char kTestData2[] = "Here it is data.";
const char kTestFileData1[] = "0123456789";
const char kTestFileData2[] = "This is sample file.";
const char kTestFileSystemFileData1[] = "abcdefghijklmnop";
const char kTestFileSystemFileData2[] = "File system file test data.";
const char kTestDataHandleData1[] = "data handle test data1.";
const char kTestDataHandleData2[] = "data handle test data2.";
const char kTestDiskCacheSideData[] = "test side data";
const char kTestContentType[] = "foo/bar";
const char kTestContentDisposition[] = "attachment; filename=foo.txt";

const char kFileSystemURLOrigin[] = "http://remote";
const storage::FileSystemType kFileSystemType =
    storage::kFileSystemTypeTemporary;

}  // namespace

class BlobURLTest : public testing::Test {
 public:
  BlobURLTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
        task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        blob_data_(std::make_unique<BlobDataBuilder>("uuid")),
        response_error_code_(net::OK),
        expected_error_code_(net::OK),
        expected_status_code_(0) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    temp_file1_ = temp_dir_.GetPath().AppendASCII("BlobFile1.dat");
    ASSERT_TRUE(base::WriteFile(temp_file1_, kTestFileData1));
    base::File::Info file_info1;
    base::GetFileInfo(temp_file1_, &file_info1);
    temp_file_modification_time1_ = file_info1.last_modified;

    temp_file2_ = temp_dir_.GetPath().AppendASCII("BlobFile2.dat");
    ASSERT_TRUE(base::WriteFile(temp_file2_, kTestFileData2));
    base::File::Info file_info2;
    base::GetFileInfo(temp_file2_, &file_info2);
    temp_file_modification_time2_ = file_info2.last_modified;
  }

  void TearDown() override {
    blob_handle_.reset();
    // Clean up for ASAN
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void SetUpFileSystem() {
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    // Prepare file system.
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        quota_manager_proxy_.get(), temp_dir_.GetPath());

    base::RunLoop run_loop;
    file_system_context_->OpenFileSystem(
        blink::StorageKey::CreateFromStringForTesting(kFileSystemURLOrigin),
        /*bucket=*/std::nullopt, kFileSystemType,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(&BlobURLTest::OnValidateFileSystem,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(file_system_root_url_.is_valid());

    // Prepare files on file system.
    const char kFilename1[] = "FileSystemFile1.dat";
    temp_file_system_file1_ = GetFileSystemURL(kFilename1);
    WriteFileSystemFile(kFilename1, kTestFileSystemFileData1,
                        &temp_file_system_file_modification_time1_);
    const char kFilename2[] = "FileSystemFile2.dat";
    temp_file_system_file2_ = GetFileSystemURL(kFilename2);
    WriteFileSystemFile(kFilename2, kTestFileSystemFileData2,
                        &temp_file_system_file_modification_time2_);
  }

  GURL GetFileSystemURL(const std::string& filename) {
    return GURL(file_system_root_url_.ToGURL().spec() + filename);
  }

  void WriteFileSystemFile(const std::string& filename,
                           std::string_view data,
                           base::Time* modification_time) {
    storage::FileSystemURL url =
        file_system_context_->CreateCrackedFileSystemURL(
            blink::StorageKey::CreateFromStringForTesting(kFileSystemURLOrigin),
            kFileSystemType, base::FilePath().AppendASCII(filename));

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_.get(), url, data));

    base::File::Info file_info;
    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::GetMetadata(
                  file_system_context_.get(), url, &file_info));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

  void OnValidateFileSystem(base::OnceClosure quit_closure,
                            const storage::FileSystemURL& root,
                            const std::string& name,
                            base::File::Error result) {
    ASSERT_EQ(base::File::FILE_OK, result);
    ASSERT_TRUE(root.is_valid());
    file_system_root_url_ = root;
    std::move(quit_closure).Run();
  }

  void TestSuccessNonrangeRequest(const std::string& expected_response,
                                  int64_t expected_content_length) {
    expected_error_code_ = net::OK;
    expected_status_code_ = 200;
    expected_response_ = expected_response;
    TestRequest("GET", net::HttpRequestHeaders());
    EXPECT_EQ(expected_content_length, response_headers_->GetContentLength());
  }

  void TestErrorRequest(int expected_error_code) {
    expected_error_code_ = expected_error_code;
    expected_response_ = "";
    TestRequest("GET", net::HttpRequestHeaders());
    EXPECT_FALSE(response_metadata_.has_value());
  }

  void TestRequest(const std::string& method,
                   const net::HttpRequestHeaders& extra_headers) {
    auto origin = url::Origin::Create(GURL("https://example.com"));
    const auto storage_key = blink::StorageKey::CreateFirstParty(origin);
    auto url = GURL("blob:" + origin.Serialize() + "/id1");
    network::ResourceRequest request;
    request.url = url;
    request.method = method;
    request.headers = extra_headers;

    storage::BlobURLStoreImpl url_store(storage_key, storage_key.origin(), 0,
                                        blob_url_registry_.AsWeakPtr());

    mojo::PendingRemote<blink::mojom::Blob> blob_remote;
    storage::BlobImpl::Create(
        std::make_unique<storage::BlobDataHandle>(*GetHandleFromBuilder()),
        blob_remote.InitWithNewPipeAndPassReceiver());

    base::RunLoop register_loop;
    base::UnguessableToken agent = base::UnguessableToken::Create();
    url_store.Register(std::move(blob_remote), url, agent,
                       net::SchemefulSite(origin), register_loop.QuitClosure());
    register_loop.Run();

    base::RunLoop resolve_loop;
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
    url_store.ResolveAsURLLoaderFactory(
        url, url_loader_factory.BindNewPipeAndPassReceiver(),
        base::BindOnce(
            [](base::OnceClosure done,
               const base::UnguessableToken& agent_registered,
               const std::optional<base::UnguessableToken>&
                   unsafe_agent_cluster_id,
               const std::optional<net::SchemefulSite>& unsafe_top_level_site) {
              EXPECT_EQ(agent_registered, unsafe_agent_cluster_id);
              std::move(done).Run();
            },
            resolve_loop.QuitClosure(), agent));
    resolve_loop.Run();

    mojo::PendingRemote<network::mojom::URLLoader> url_loader;
    network::TestURLLoaderClient url_loader_client;
    url_loader_factory->CreateLoaderAndStart(
        url_loader.InitWithNewPipeAndPassReceiver(), 0,
        network::mojom::kURLLoadOptionNone, request,
        url_loader_client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    url_loader_client.RunUntilComplete();

    if (url_loader_client.response_body().is_valid()) {
      EXPECT_TRUE(mojo::BlockingCopyToString(
          url_loader_client.response_body_release(), &response_));
    }
    response_headers_ = url_loader_client.response_head()
                            ? url_loader_client.response_head()->headers
                            : nullptr;
    response_metadata_ = url_loader_client.cached_metadata();
    response_error_code_ = url_loader_client.completion_status().error_code;

    // Verify response.
    EXPECT_EQ(expected_error_code_, response_error_code_);
    if (response_error_code_ == net::OK) {
      EXPECT_EQ(expected_status_code_, response_headers_->response_code());
      EXPECT_EQ(expected_response_, response_);
    }
  }

  void BuildComplicatedData(std::string* expected_result) {
    auto str1 = std::string(kTestData1 + 1, 2);
    blob_data_->AppendData(str1);
    *expected_result = str1;

    blob_data_->AppendFile(temp_file1_, 2, 3, temp_file_modification_time1_);
    *expected_result += std::string(kTestFileData1 + 2, 3);

    blob_data_->AppendReadableDataHandle(
        base::MakeRefCounted<storage::FakeBlobDataHandle>(kTestDataHandleData1,
                                                          ""));
    *expected_result += std::string(kTestDataHandleData1);

    blob_data_->AppendFileSystemFile(
        file_system_context_->CrackURLInFirstPartyContext(
            temp_file_system_file1_),
        3, 4, temp_file_system_file_modification_time1_, file_system_context_);
    *expected_result += std::string(kTestFileSystemFileData1 + 3, 4);

    auto str2 = std::string(kTestData2 + 4, 5);
    blob_data_->AppendData(str2);
    *expected_result += str2;

    blob_data_->AppendFile(temp_file2_, 5, 6, temp_file_modification_time2_);
    *expected_result += std::string(kTestFileData2 + 5, 6);

    blob_data_->AppendFileSystemFile(
        file_system_context_->CrackURLInFirstPartyContext(
            temp_file_system_file2_),
        6, 7, temp_file_system_file_modification_time2_, file_system_context_);
    *expected_result += std::string(kTestFileSystemFileData2 + 6, 7);
  }

  storage::BlobDataHandle* GetHandleFromBuilder() {
    if (!blob_handle_) {
      blob_handle_ = blob_context_.AddFinishedBlob(std::move(blob_data_));
    }
    return blob_handle_.get();
  }

  // This only works if all the Blob items have a definite pre-computed length.
  // Otherwise, this will fail a CHECK.
  int64_t GetTotalBlobLength() {
    int64_t total = 0;
    std::unique_ptr<BlobDataSnapshot> data =
        GetHandleFromBuilder()->CreateSnapshot();
    const auto& items = data->items();
    for (const auto& item : items) {
      int64_t length = base::checked_cast<int64_t>(item->length());
      CHECK(length <= std::numeric_limits<int64_t>::max() - total);
      total += length;
    }
    return total;
  }

 protected:
  base::WeakPtr<storage::BlobStorageContext> GetStorageContext() {
    return blob_context_.AsWeakPtr();
  }

  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file1_;
  base::FilePath temp_file2_;
  base::Time temp_file_modification_time1_;
  base::Time temp_file_modification_time2_;
  storage::FileSystemURL file_system_root_url_;
  GURL temp_file_system_file1_;
  GURL temp_file_system_file2_;
  base::Time temp_file_system_file_modification_time1_;
  base::Time temp_file_system_file_modification_time2_;

  BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;

  storage::BlobStorageContext blob_context_;
  storage::BlobUrlRegistry blob_url_registry_;
  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  std::unique_ptr<BlobDataBuilder> blob_data_;
  std::unique_ptr<BlobDataSnapshot> blob_data_snapshot_;
  std::string response_;
  int response_error_code_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  std::optional<std::string> response_metadata_;

  int expected_error_code_;
  int expected_status_code_;
  std::string expected_response_;
};

TEST_F(BlobURLTest, TestGetSimpleDataRequest) {
  blob_data_->AppendData(kTestData1);
  TestSuccessNonrangeRequest(kTestData1, std::size(kTestData1) - 1);
}

TEST_F(BlobURLTest, TestGetSimpleFileRequest) {
  blob_data_->AppendFile(temp_file1_, 0, std::numeric_limits<uint64_t>::max(),
                         base::Time());
  TestSuccessNonrangeRequest(kTestFileData1, std::size(kTestFileData1) - 1);
}

TEST_F(BlobURLTest, TestGetLargeFileRequest) {
  base::FilePath large_temp_file =
      temp_dir_.GetPath().AppendASCII("LargeBlob.dat");
  std::string large_data;
  large_data.reserve(kBufferSize * 5);
  for (int i = 0; i < kBufferSize * 5; ++i)
    large_data.append(1, static_cast<char>(i % 256));
  ASSERT_TRUE(base::WriteFile(large_temp_file, large_data));
  blob_data_->AppendFile(large_temp_file, 0,
                         std::numeric_limits<uint64_t>::max(), base::Time());
  TestSuccessNonrangeRequest(large_data, large_data.size());
}

TEST_F(BlobURLTest, TestGetNonExistentFileRequest) {
  base::FilePath non_existent_file =
      temp_file1_.InsertBeforeExtension(FILE_PATH_LITERAL("-na"));
  blob_data_->AppendFile(non_existent_file, 0,
                         std::numeric_limits<uint64_t>::max(), base::Time());
  TestErrorRequest(net::ERR_FILE_NOT_FOUND);
}

TEST_F(BlobURLTest, TestGetChangedFileRequest) {
  base::Time old_time = temp_file_modification_time1_ - base::Seconds(10);
  blob_data_->AppendFile(temp_file1_, 0, 3, old_time);
  TestErrorRequest(net::ERR_UPLOAD_FILE_CHANGED);
}

TEST_F(BlobURLTest, TestGetSlicedFileRequest) {
  blob_data_->AppendFile(temp_file1_, 2, 4, temp_file_modification_time1_);
  std::string result(kTestFileData1 + 2, 4);
  TestSuccessNonrangeRequest(result, 4);
}

TEST_F(BlobURLTest, TestGetSimpleFileSystemFileRequest) {
  SetUpFileSystem();
  blob_data_->AppendFileSystemFile(
      file_system_context_->CrackURLInFirstPartyContext(
          temp_file_system_file1_),
      0, std::numeric_limits<uint64_t>::max(), base::Time(),
      file_system_context_);
  TestSuccessNonrangeRequest(kTestFileSystemFileData1,
                             std::size(kTestFileSystemFileData1) - 1);
}

TEST_F(BlobURLTest, TestGetLargeFileSystemFileRequest) {
  SetUpFileSystem();
  std::string large_data;
  large_data.reserve(kBufferSize * 5);
  for (int i = 0; i < kBufferSize * 5; ++i)
    large_data.append(1, static_cast<char>(i % 256));

  const char kFilename[] = "LargeBlob.dat";
  WriteFileSystemFile(kFilename, large_data, nullptr);

  blob_data_->AppendFileSystemFile(
      file_system_context_->CrackURLInFirstPartyContext(
          GetFileSystemURL(kFilename)),
      0, std::numeric_limits<uint64_t>::max(), base::Time(),
      file_system_context_);
  TestSuccessNonrangeRequest(large_data, large_data.size());
}

TEST_F(BlobURLTest, TestGetNonExistentFileSystemFileRequest) {
  SetUpFileSystem();
  GURL non_existent_file = GetFileSystemURL("non-existent.dat");
  blob_data_->AppendFileSystemFile(
      file_system_context_->CrackURLInFirstPartyContext(non_existent_file), 0,
      std::numeric_limits<uint64_t>::max(), base::Time(), file_system_context_);
  TestErrorRequest(net::ERR_FILE_NOT_FOUND);
}

TEST_F(BlobURLTest, TestGetInvalidFileSystemFileRequest) {
  SetUpFileSystem();
  GURL invalid_file;
  blob_data_->AppendFileSystemFile(
      file_system_context_->CrackURLInFirstPartyContext(invalid_file), 0,
      std::numeric_limits<uint64_t>::max(), base::Time(), file_system_context_);
  TestErrorRequest(net::ERR_FILE_NOT_FOUND);
}

TEST_F(BlobURLTest, TestGetChangedFileSystemFileRequest) {
  SetUpFileSystem();
  base::Time old_time =
      temp_file_system_file_modification_time1_ - base::Seconds(10);
  blob_data_->AppendFileSystemFile(
      file_system_context_->CrackURLInFirstPartyContext(
          temp_file_system_file1_),
      0, 3, old_time, file_system_context_);
  TestErrorRequest(net::ERR_UPLOAD_FILE_CHANGED);
}

TEST_F(BlobURLTest, TestGetSlicedFileSystemFileRequest) {
  SetUpFileSystem();
  blob_data_->AppendFileSystemFile(
      file_system_context_->CrackURLInFirstPartyContext(
          temp_file_system_file1_),
      2, 4, temp_file_system_file_modification_time1_, file_system_context_);
  std::string result(kTestFileSystemFileData1 + 2, 4);
  TestSuccessNonrangeRequest(result, 4);
}

TEST_F(BlobURLTest, TestGetSimpleDataHandleRequest) {
  blob_data_->AppendReadableDataHandle(
      base::MakeRefCounted<storage::FakeBlobDataHandle>(kTestDataHandleData1,
                                                        ""));
  TestSuccessNonrangeRequest(kTestDataHandleData1,
                             std::size(kTestDataHandleData1) - 1);
}

TEST_F(BlobURLTest, TestGetComplicatedDataFileAndDiskCacheRequest) {
  SetUpFileSystem();
  std::string result;
  BuildComplicatedData(&result);
  TestSuccessNonrangeRequest(result, GetTotalBlobLength());
}

TEST_F(BlobURLTest, TestGetRangeRequest1) {
  SetUpFileSystem();
  std::string result;
  BuildComplicatedData(&result);
  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange,
                          net::HttpByteRange::Bounded(5, 10).GetHeaderValue());
  expected_status_code_ = 206;
  expected_response_ = result.substr(5, 10 - 5 + 1);
  TestRequest("GET", extra_headers);

  EXPECT_EQ(6, response_headers_->GetContentLength());
  EXPECT_FALSE(response_metadata_.has_value());

  int64_t first = 0, last = 0, length = 0;
  EXPECT_TRUE(response_headers_->GetContentRangeFor206(&first, &last, &length));
  EXPECT_EQ(5, first);
  EXPECT_EQ(10, last);
  EXPECT_EQ(GetTotalBlobLength(), length);
}

TEST_F(BlobURLTest, TestGetRangeRequest2) {
  SetUpFileSystem();
  std::string result;
  BuildComplicatedData(&result);
  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange,
                          net::HttpByteRange::Suffix(10).GetHeaderValue());
  expected_status_code_ = 206;
  expected_response_ = result.substr(result.length() - 10);
  TestRequest("GET", extra_headers);

  EXPECT_EQ(10, response_headers_->GetContentLength());
  EXPECT_FALSE(response_metadata_.has_value());

  int64_t total = GetTotalBlobLength();
  int64_t first = 0, last = 0, length = 0;
  EXPECT_TRUE(response_headers_->GetContentRangeFor206(&first, &last, &length));
  EXPECT_EQ(total - 10, first);
  EXPECT_EQ(total - 1, last);
  EXPECT_EQ(total, length);
}

TEST_F(BlobURLTest, TestGetRangeRequest3) {
  SetUpFileSystem();
  std::string result;
  BuildComplicatedData(&result);
  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange,
                          net::HttpByteRange::Bounded(0, 2).GetHeaderValue());
  expected_status_code_ = 206;
  expected_response_ = result.substr(0, 3);
  TestRequest("GET", extra_headers);

  EXPECT_EQ(3, response_headers_->GetContentLength());
  EXPECT_FALSE(response_metadata_.has_value());

  int64_t first = 0, last = 0, length = 0;
  EXPECT_TRUE(response_headers_->GetContentRangeFor206(&first, &last, &length));
  EXPECT_EQ(0, first);
  EXPECT_EQ(2, last);
  EXPECT_EQ(GetTotalBlobLength(), length);
}

TEST_F(BlobURLTest, TestExtraHeaders) {
  blob_data_->set_content_type(kTestContentType);
  blob_data_->set_content_disposition(kTestContentDisposition);
  blob_data_->AppendData(kTestData1);
  expected_status_code_ = 200;
  expected_response_ = kTestData1;
  TestRequest("GET", net::HttpRequestHeaders());

  std::string content_type;
  EXPECT_TRUE(response_headers_->GetMimeType(&content_type));
  EXPECT_EQ(kTestContentType, content_type);
  EXPECT_FALSE(response_metadata_.has_value());
  size_t iter = 0;
  std::string content_disposition;
  EXPECT_TRUE(response_headers_->EnumerateHeader(&iter, "Content-Disposition",
                                                 &content_disposition));
  EXPECT_EQ(kTestContentDisposition, content_disposition);
}

TEST_F(BlobURLTest, TestSideData) {
  blob_data_->AppendReadableDataHandle(
      base::MakeRefCounted<storage::FakeBlobDataHandle>(
          kTestDataHandleData2, kTestDiskCacheSideData));
  expected_status_code_ = 200;
  expected_response_ = kTestDataHandleData2;
  TestRequest("GET", net::HttpRequestHeaders());
  EXPECT_EQ(static_cast<int>(std::size(kTestDataHandleData2) - 1),
            response_headers_->GetContentLength());

  EXPECT_EQ(std::string(kTestDiskCacheSideData), *response_metadata_);
}

TEST_F(BlobURLTest, TestZeroSizeSideData) {
  blob_data_->AppendReadableDataHandle(
      base::MakeRefCounted<storage::FakeBlobDataHandle>(kTestDataHandleData2,
                                                        ""));
  expected_status_code_ = 200;
  expected_response_ = kTestDataHandleData2;
  TestRequest("GET", net::HttpRequestHeaders());
  EXPECT_EQ(static_cast<int>(std::size(kTestDataHandleData2) - 1),
            response_headers_->GetContentLength());

  EXPECT_FALSE(response_metadata_.has_value());
}

TEST_F(BlobURLTest, BrokenBlob) {
  blob_handle_ = blob_context_.AddBrokenBlob(
      "uuid", "", "", storage::BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
  TestErrorRequest(net::ERR_FAILED);
}

}  // namespace content
