// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_writer_impl.h"

#include <limits>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"
#include "content/browser/native_file_system/mock_native_file_system_permission_context.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using blink::mojom::NativeFileSystemStatus;
using storage::FileSystemURL;
using storage::IsolatedContext;

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Field;

namespace content {

std::string GetHexEncodedString(const std::string& input) {
  return base::HexEncode(base::as_bytes(base::make_span(input)));
}

class NativeFileSystemFileWriterImplTest : public testing::Test {
 public:
  NativeFileSystemFileWriterImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);
  }

  virtual NativeFileSystemPermissionContext* permission_context() {
    return nullptr;
  }

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    auto* isolated_context = IsolatedContext::GetInstance();
    std::string base_name;
    IsolatedContext::ScopedFSHandle fs =
        isolated_context->RegisterFileSystemForPath(
            storage::kFileSystemTypeNativeLocal, std::string(), dir_.GetPath(),
            &base_name);
    base::FilePath root_path =
        isolated_context->CreateVirtualRootPath(fs.id()).AppendASCII(base_name);

    test_file_url_ = file_system_context_->CreateCrackedFileSystemURL(
        kTestOrigin.GetURL(), storage::kFileSystemTypeIsolated,
        root_path.AppendASCII("test"));

    test_swap_url_ = file_system_context_->CreateCrackedFileSystemURL(
        kTestOrigin.GetURL(), storage::kFileSystemTypeIsolated,
        root_path.AppendASCII("test.crswap"));

    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                              test_file_url_));

    ASSERT_EQ(base::File::FILE_OK,
              AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                              test_swap_url_));

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(), nullptr);
    blob_context_ = chrome_blob_context_->context();

    manager_ = base::MakeRefCounted<NativeFileSystemManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/permission_context(),
        /*off_the_record=*/false);

    handle_ = std::make_unique<NativeFileSystemFileWriterImpl>(
        manager_.get(),
        NativeFileSystemManagerImpl::BindingContext(kTestOrigin, kTestURL,
                                                    kProcessId, kFrameId),
        test_file_url_, test_swap_url_,
        NativeFileSystemManagerImpl::SharedHandleState(
            permission_grant_, permission_grant_, std::move(fs)),
        /*has_transient_user_activation=*/false);
    handle_->SetSkipQuarantineCheckForTesting();
  }

  void TearDown() override {
    handle_.reset();
    manager_.reset();

    task_environment_.RunUntilIdle();
    EXPECT_TRUE(dir_.Delete());
  }

  mojo::PendingRemote<blink::mojom::Blob> CreateBlob(
      const std::string& contents) {
    auto builder =
        std::make_unique<storage::BlobDataBuilder>(base::GenerateGUID());
    builder->AppendData(contents);
    auto handle = blob_context_->AddFinishedBlob(std::move(builder));
    mojo::PendingRemote<blink::mojom::Blob> result;
    storage::BlobImpl::Create(std::move(handle),
                              result.InitWithNewPipeAndPassReceiver());
    return result;
  }

  mojo::ScopedDataPipeConsumerHandle CreateStream(const std::string& contents) {
    // Test with a relatively low capacity pipe to make sure it isn't all
    // written/read in one go.
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 16;
    mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
    CHECK(producer_handle.is_valid());
    auto producer =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    auto* producer_raw = producer.get();
    producer_raw->Write(
        std::make_unique<mojo::StringDataSource>(
            contents, mojo::StringDataSource::AsyncWritingMode::
                          STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
        base::BindOnce(
            base::DoNothing::Once<std::unique_ptr<mojo::DataPipeProducer>,
                                  MojoResult>(),
            std::move(producer)));
    return consumer_handle;
  }

  std::string ReadFile(const FileSystemURL& url) {
    std::unique_ptr<storage::FileStreamReader> reader =
        file_system_context_->CreateFileStreamReader(
            url, 0, std::numeric_limits<int64_t>::max(), base::Time());
    std::string result;
    while (true) {
      auto buf = base::MakeRefCounted<net::IOBufferWithSize>(4096);
      net::TestCompletionCallback callback;
      int rv = reader->Read(buf.get(), buf->size(), callback.callback());
      if (rv == net::ERR_IO_PENDING)
        rv = callback.WaitForResult();
      EXPECT_GE(rv, 0);
      if (rv < 0)
        return "(read failure)";
      if (rv == 0)
        return result;
      result.append(buf->data(), rv);
    }
  }

  NativeFileSystemStatus WriteBlobSync(
      uint64_t position,
      mojo::PendingRemote<blink::mojom::Blob> blob,
      uint64_t* bytes_written_out) {
    base::RunLoop loop;
    NativeFileSystemStatus result_out;
    handle_->Write(position, std::move(blob),
                   base::BindLambdaForTesting(
                       [&](blink::mojom::NativeFileSystemErrorPtr result,
                           uint64_t bytes_written) {
                         result_out = result->status;
                         *bytes_written_out = bytes_written;
                         loop.Quit();
                       }));
    loop.Run();
    return result_out;
  }

  NativeFileSystemStatus WriteStreamSync(
      uint64_t position,
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      uint64_t* bytes_written_out) {
    base::RunLoop loop;
    NativeFileSystemStatus result_out;
    handle_->WriteStream(position, std::move(data_pipe),
                         base::BindLambdaForTesting(
                             [&](blink::mojom::NativeFileSystemErrorPtr result,
                                 uint64_t bytes_written) {
                               result_out = result->status;
                               *bytes_written_out = bytes_written;
                               loop.Quit();
                             }));
    loop.Run();
    return result_out;
  }

  NativeFileSystemStatus TruncateSync(uint64_t length) {
    base::RunLoop loop;
    NativeFileSystemStatus result_out;
    handle_->Truncate(length,
                      base::BindLambdaForTesting(
                          [&](blink::mojom::NativeFileSystemErrorPtr result) {
                            result_out = result->status;
                            loop.Quit();
                          }));
    loop.Run();
    return result_out;
  }

  NativeFileSystemStatus CloseSync() {
    base::RunLoop loop;
    NativeFileSystemStatus result_out;
    handle_->Close(base::BindLambdaForTesting(
        [&](blink::mojom::NativeFileSystemErrorPtr result) {
          result_out = result->status;
          loop.Quit();
        }));
    loop.Run();
    return result_out;
  }

  virtual bool WriteUsingBlobs() { return true; }

  NativeFileSystemStatus WriteSync(uint64_t position,
                                   const std::string& contents,
                                   uint64_t* bytes_written_out) {
    if (WriteUsingBlobs())
      return WriteBlobSync(position, CreateBlob(contents), bytes_written_out);
    return WriteStreamSync(position, CreateStream(contents), bytes_written_out);
  }

 protected:
  const GURL kTestURL = GURL("https://example.com/test");
  const url::Origin kTestOrigin = url::Origin::Create(kTestURL);
  const int kProcessId = 1;
  const int kFrameId = 2;
  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  storage::BlobStorageContext* blob_context_;
  scoped_refptr<NativeFileSystemManagerImpl> manager_;

  FileSystemURL test_file_url_;
  FileSystemURL test_swap_url_;

  scoped_refptr<FixedNativeFileSystemPermissionGrant> permission_grant_ =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          FixedNativeFileSystemPermissionGrant::PermissionStatus::GRANTED);
  std::unique_ptr<NativeFileSystemFileWriterImpl> handle_;
};

class NativeFileSystemFileWriterImplWriteTest
    : public NativeFileSystemFileWriterImplTest,
      public testing::WithParamInterface<bool> {
 public:
  bool WriteUsingBlobs() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(NativeFileSystemFileWriterImplTest,
                         NativeFileSystemFileWriterImplWriteTest,
                         ::testing::Bool());

TEST_F(NativeFileSystemFileWriterImplTest, WriteInvalidBlob) {
  // This test primarily verifies behavior of the browser process in the
  // presence of a compromised renderer process. The situation this tests for
  // normally can't occur. As such it doesn't really matter what status the
  // write operation returns, the important part is that nothing crashes.

  mojo::PendingRemote<blink::mojom::Blob> blob;
  ignore_result(blob.InitWithNewPipeAndPassReceiver());

  uint64_t bytes_written;
  NativeFileSystemStatus result =
      WriteBlobSync(0, std::move(blob), &bytes_written);
  EXPECT_EQ(bytes_written, 0u);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  EXPECT_EQ("", ReadFile(test_file_url_));
}

TEST_F(NativeFileSystemFileWriterImplTest, HashSimpleOK) {
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  base::RunLoop loop;
  handle_->ComputeHashForSwapFileForTesting(base::BindLambdaForTesting(
      [&](base::File::Error result, const std::string& hash_value,
          int64_t size) {
        EXPECT_EQ(base::File::FILE_OK, result);
        EXPECT_EQ(
            "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
            GetHexEncodedString(hash_value));
        EXPECT_EQ(3, size);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(NativeFileSystemFileWriterImplTest, HashEmptyOK) {
  base::RunLoop loop;
  handle_->ComputeHashForSwapFileForTesting(base::BindLambdaForTesting(
      [&](base::File::Error result, const std::string& hash_value,
          int64_t size) {
        EXPECT_EQ(base::File::FILE_OK, result);
        EXPECT_EQ(
            "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855",
            GetHexEncodedString(hash_value));
        EXPECT_EQ(0, size);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(NativeFileSystemFileWriterImplTest, HashNonExistingFileFails) {
  ASSERT_EQ(base::File::FILE_OK,
            AsyncFileTestHelper::Remove(file_system_context_.get(),
                                        handle_->swap_url(),
                                        /*recursive=*/false));
  base::RunLoop loop;
  handle_->ComputeHashForSwapFileForTesting(base::BindLambdaForTesting(
      [&](base::File::Error result, const std::string& hash_value,
          int64_t size) {
        EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, result);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(NativeFileSystemFileWriterImplTest, HashLargerFileOK) {
  size_t target_size = 9 * 1024u;
  std::string file_data(target_size, '0');
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, file_data, &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, target_size);

  base::RunLoop loop;
  handle_->ComputeHashForSwapFileForTesting(base::BindLambdaForTesting(
      [&](base::File::Error result, const std::string& hash_value,
          int64_t size) {
        EXPECT_EQ(base::File::FILE_OK, result);
        EXPECT_EQ(
            "34A82D28CB1E0BA92CADC4BE8497DC9EEA9AC4F63B9C445A9E52D298990AC491",
            GetHexEncodedString(hash_value));
        EXPECT_EQ(int64_t{target_size}, size);
        loop.Quit();
      }));
  loop.Run();
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteValidEmptyString) {
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, "", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 0u);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  EXPECT_EQ("", ReadFile(test_file_url_));
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteValidNonEmpty) {
  std::string test_data("abcdefghijklmnopqrstuvwxyz");
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, test_data, &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, test_data.size());

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  EXPECT_EQ(test_data, ReadFile(test_file_url_));
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteWithOffsetInFile) {
  uint64_t bytes_written;
  NativeFileSystemStatus result;

  result = WriteSync(0, "1234567890", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 10u);

  result = WriteSync(4, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  EXPECT_EQ("1234abc890", ReadFile(test_file_url_));
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteWithOffsetPastFile) {
  // TODO(https://crbug.com/998913): Currently expectations here are different
  // from what WPT tests are asserting. Figure out what the desired behavior is
  // and make both tests match.
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(4, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  using std::string_literals::operator""s;
  EXPECT_EQ("\0\0\0\0abc"s, ReadFile(test_file_url_));
}

TEST_F(NativeFileSystemFileWriterImplTest, TruncateShrink) {
  uint64_t bytes_written;
  NativeFileSystemStatus result;

  result = WriteSync(0, "1234567890", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 10u);

  result = TruncateSync(5);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  EXPECT_EQ("12345", ReadFile(test_file_url_));
}

TEST_F(NativeFileSystemFileWriterImplTest, TruncateGrow) {
  uint64_t bytes_written;
  NativeFileSystemStatus result;

  result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = TruncateSync(5);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  EXPECT_EQ(std::string("abc\0\0", 5), ReadFile(test_file_url_));
}

TEST_F(NativeFileSystemFileWriterImplTest, CloseAfterCloseNotOK) {
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kInvalidState);
}

TEST_F(NativeFileSystemFileWriterImplTest, TruncateAfterCloseNotOK) {
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  result = TruncateSync(0);
  EXPECT_EQ(result, NativeFileSystemStatus::kInvalidState);
}

TEST_P(NativeFileSystemFileWriterImplWriteTest, WriteAfterCloseNotOK) {
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  result = WriteSync(0, "bcd", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kInvalidState);
}

// TODO(mek): More tests, particularly for error conditions.

class NativeFileSystemFileWriterAfterWriteChecksTest
    : public NativeFileSystemFileWriterImplTest {
 public:
  NativeFileSystemPermissionContext* permission_context() override {
    return &permission_context_;
  }

 protected:
  testing::StrictMock<MockNativeFileSystemPermissionContext>
      permission_context_;
};

TEST_F(NativeFileSystemFileWriterAfterWriteChecksTest, Allow) {
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  std::string expected_hash;
  ASSERT_TRUE(base::HexStringToString(
      "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
      &expected_hash));

  EXPECT_CALL(
      permission_context_,
      PerformAfterWriteChecks_(
          AllOf(
              Field(&NativeFileSystemWriteItem::target_file_path,
                    Eq(test_file_url_.path())),
              Field(&NativeFileSystemWriteItem::full_path,
                    Eq(test_swap_url_.path())),
              Field(&NativeFileSystemWriteItem::sha256_hash, Eq(expected_hash)),
              Field(&NativeFileSystemWriteItem::size, Eq(3)),
              Field(&NativeFileSystemWriteItem::frame_url, Eq(kTestURL)),
              Field(&NativeFileSystemWriteItem::has_user_gesture, Eq(false))),
          kProcessId, kFrameId, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          NativeFileSystemPermissionContext::AfterWriteCheckResult::kAllow));

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(file_system_context_.get(),
                                              test_file_url_, 3));
}

TEST_F(NativeFileSystemFileWriterAfterWriteChecksTest, Block) {
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  EXPECT_CALL(permission_context_,
              PerformAfterWriteChecks_(_, kProcessId, kFrameId, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          NativeFileSystemPermissionContext::AfterWriteCheckResult::kBlock));

  result = CloseSync();
  EXPECT_EQ(result, NativeFileSystemStatus::kOperationAborted);

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(file_system_context_.get(),
                                              test_file_url_, 0));
}

TEST_F(NativeFileSystemFileWriterAfterWriteChecksTest, HandleCloseDuringCheck) {
  uint64_t bytes_written;
  NativeFileSystemStatus result = WriteSync(0, "abc", &bytes_written);
  EXPECT_EQ(result, NativeFileSystemStatus::kOk);
  EXPECT_EQ(bytes_written, 3u);

  using SBCallback = base::OnceCallback<void(
      NativeFileSystemPermissionContext::AfterWriteCheckResult)>;
  SBCallback sb_callback;
  base::RunLoop loop;
  EXPECT_CALL(permission_context_, PerformAfterWriteChecks_)
      .WillOnce(
          testing::Invoke([&](NativeFileSystemWriteItem* item, int process_id,
                              int frame_id, SBCallback& callback) {
            sb_callback = std::move(callback);
            loop.Quit();
          }));

  handle_->Close(base::DoNothing());
  loop.Run();

  handle_.reset();
  // Destructor should not have deleted swap file with an active safe browsing
  // check pending.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      AsyncFileTestHelper::kDontCheckSize));

  std::move(sb_callback)
      .Run(NativeFileSystemPermissionContext::AfterWriteCheckResult::kAllow);

  // Swap file should now be deleted, target file should be unmodified.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url_,
      AsyncFileTestHelper::kDontCheckSize));
  EXPECT_TRUE(AsyncFileTestHelper::FileExists(file_system_context_.get(),
                                              test_file_url_, 0));
}

}  // namespace content
