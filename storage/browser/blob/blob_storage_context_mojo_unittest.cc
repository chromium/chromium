// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "storage/browser/blob/blob_storage_context.h"

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace storage {
namespace {

class DataPipeReader : public mojo::DataPipeDrainer::Client {
 public:
  DataPipeReader(std::string* data_out, base::OnceClosure done_callback)
      : data_out_(data_out), done_callback_(std::move(done_callback)) {}

  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_out_->append(base::as_string_view(data));
  }

  void OnDataComplete() override { std::move(done_callback_).Run(); }

 private:
  raw_ptr<std::string> data_out_;
  base::OnceClosure done_callback_;
};

std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle pipe) {
  base::RunLoop loop;
  std::string data;
  DataPipeReader reader(&data, loop.QuitClosure());
  mojo::DataPipeDrainer drainer(&reader, std::move(pipe));
  loop.Run();
  return data;
}

class BlobStorageContextMojoTest : public testing::Test {
 protected:
  BlobStorageContextMojoTest() = default;
  ~BlobStorageContextMojoTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    disallow_blocking_.emplace();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    disallow_blocking_.reset();
    ASSERT_TRUE(!temp_dir_.IsValid() || temp_dir_.Delete());
  }

  void SetUpOnDiskContext() {
    if (!file_runner_) {
      file_runner_ =
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
    }
    context_ = std::make_unique<BlobStorageContext>(
        temp_dir_.GetPath(), temp_dir_.GetPath(), file_runner_);
  }

  void SetUpInMemoryContext() {
    context_ = std::make_unique<BlobStorageContext>();
  }

  mojo::Remote<mojom::BlobStorageContext> CreateContextConnection() {
    mojo::Remote<mojom::BlobStorageContext> remote_context;
    context_->Bind(remote_context.BindNewPipeAndPassReceiver());
    return remote_context;
  }

  std::string UUIDFromBlob(blink::mojom::Blob* blob) {
    base::RunLoop loop;
    std::string received_uuid;
    blob->GetInternalUUID(base::BindOnce(
        [](base::OnceClosure quit_closure, std::string* uuid_out,
           const std::string& uuid) {
          *uuid_out = uuid;
          std::move(quit_closure).Run();
        },
        loop.QuitClosure(), &received_uuid));
    loop.Run();
    return received_uuid;
  }

  // This is used because Mac saves file modification timestamps in second
  // granularity.
  base::Time TruncateToSeconds(base::Time time) {
    base::Time::Exploded exploded;
    time.UTCExplode(&exploded);
    exploded.millisecond = 0;
    EXPECT_TRUE(base::Time::FromUTCExploded(exploded, &time));
    return time;
  }

  void CreateFile(base::FilePath path,
                  std::string data,
                  std::optional<base::Time> modification_time) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(path, data));
    if (modification_time) {
      ASSERT_TRUE(base::TouchFile(path, modification_time.value(),
                                  modification_time.value()));
    }
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  scoped_refptr<base::SequencedTaskRunner> file_runner_;
  std::unique_ptr<BlobStorageContext> context_;
  std::optional<base::ScopedDisallowBlocking> disallow_blocking_;
};

TEST_F(BlobStorageContextMojoTest, BasicBlobCreation) {
  SetUpInMemoryContext();
  const std::string kData = "Hello There!";
  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  mojo::Remote<blink::mojom::Blob> blob;
  context->RegisterFromMemory(
      blob.BindNewPipeAndPassReceiver(), "1234",
      mojo_base::BigBuffer(base::as_bytes(base::make_span(kData))));

  EXPECT_EQ(std::string("1234"), UUIDFromBlob(blob.get()));

  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                                 data_pipe_consumer));
  blob->ReadAll(std::move(data_pipe_producer), mojo::NullRemote());
  std::string received = ReadDataPipe(std::move(data_pipe_consumer));
  EXPECT_EQ(std::string(kData), received);
}

TEST_F(BlobStorageContextMojoTest, SaveBlobToFile) {
  SetUpOnDiskContext();
  const std::string kData = "Hello There!";
  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  mojo::Remote<blink::mojom::Blob> blob;
  context->RegisterFromMemory(
      blob.BindNewPipeAndPassReceiver(), "1234",
      mojo_base::BigBuffer(base::as_bytes(base::make_span(kData))));

  // Create a 'last modified' that is different from now.
  base::Time last_modified =
      TruncateToSeconds(base::Time::Now() - base::Days(1));

  base::RunLoop loop;
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("TestFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, last_modified,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kSuccess);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, kData);

  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(file_path, &file_info));

  // Because Mac rounds file modification time to the nearest second, make sure
  // the difference is within that range.
  base::TimeDelta difference = file_info.last_modified - last_modified;
  EXPECT_LT(difference.magnitude(), base::Seconds(1));

  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, SaveBlobToFileNoDate) {
  SetUpOnDiskContext();
  const std::string kData = "Hello There!";
  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  mojo::Remote<blink::mojom::Blob> blob;
  context->RegisterFromMemory(
      blob.BindNewPipeAndPassReceiver(), "1234",
      mojo_base::BigBuffer(base::as_bytes(base::make_span(kData))));

  base::RunLoop loop;
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("TestFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, std::nullopt,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kSuccess);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, kData);

  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, SaveEmptyBlobToFile) {
  SetUpOnDiskContext();
  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  mojo::Remote<blink::mojom::Blob> blob;
  context->RegisterFromMemory(blob.BindNewPipeAndPassReceiver(), "1234",
                              mojo_base::BigBuffer());

  // Create a 'last modified' that is different from now.
  base::Time last_modified =
      TruncateToSeconds(base::Time::Now() - base::Days(1));

  base::RunLoop loop;
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("TestFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, last_modified,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kSuccess);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, std::string(""));

  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(file_path, &file_info));

  // Because Mac rounds file modification time to the nearest second, make sure
  // the difference is within that range.
  base::TimeDelta difference = file_info.last_modified - last_modified;
  EXPECT_LT(difference.magnitude(), base::Seconds(1));

  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, FileCopyOptimization) {
  SetUpOnDiskContext();
  const std::string kData = "Hello There!";

  base::FilePath copy_from_file =
      temp_dir_.GetPath().AppendASCII("SourceFile.txt");

  // Create a file to copy from.
  base::Time modification_time =
      TruncateToSeconds(base::Time::Now() - base::Days(1));
  CreateFile(copy_from_file, kData, modification_time);

  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>("1234");
  builder->AppendFile(copy_from_file, 0, kData.size(), modification_time);
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->AddFinishedBlob(std::move(builder));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  BlobImpl::Create(std::move(blob_handle),
                   blob.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  base::RunLoop loop;
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("DestinationFile.txt");
  context->WriteBlobToFile(
      std::move(blob), file_path, true, modification_time,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kSuccess);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, kData);

  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(file_path, &file_info));

  // Because Mac rounds file modification time to the nearest second, make sure
  // the difference is within that range.
  base::TimeDelta difference = file_info.last_modified - modification_time;
  EXPECT_LT(difference.magnitude(), base::Seconds(1));

  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, FileCopyOptimizationOffsetSize) {
  SetUpOnDiskContext();
  static const std::string kData = "Hello There!";
  static const int64_t kOffset = 1;
  static const int64_t kSize = kData.size() - 2;

  base::FilePath copy_from_file =
      temp_dir_.GetPath().AppendASCII("SourceFile.txt");

  // Create a file to copy from.
  base::Time modification_time =
      TruncateToSeconds(base::Time::Now() - base::Days(1));
  CreateFile(copy_from_file, kData, modification_time);

  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>("1234");
  builder->AppendFile(copy_from_file, kOffset, kSize, modification_time);
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->AddFinishedBlob(std::move(builder));

  mojo::Remote<blink::mojom::Blob> blob;
  BlobImpl::Create(std::move(blob_handle), blob.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  base::RunLoop loop;
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("DestinationFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, modification_time,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kSuccess);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, kData.substr(kOffset, kSize));

  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(file_path, &file_info));

  // Because Mac rounds file modification time to the nearest second, make sure
  // the difference is within that range.
  base::TimeDelta difference = file_info.last_modified - modification_time;
  EXPECT_LT(difference.magnitude(), base::Seconds(1));

  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, FileCopyEmptyFile) {
  SetUpOnDiskContext();
  static const std::string kData = "";

  base::FilePath copy_from_file =
      temp_dir_.GetPath().AppendASCII("SourceFile.txt");

  // Create a file to copy from.
  base::Time modification_time =
      TruncateToSeconds(base::Time::Now() - base::Days(1));
  CreateFile(copy_from_file, kData, modification_time);

  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>("1234");
  builder->AppendFile(copy_from_file, 0ll, 0ll, modification_time);
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->AddFinishedBlob(std::move(builder));

  mojo::Remote<blink::mojom::Blob> blob;
  BlobImpl::Create(std::move(blob_handle), blob.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  base::RunLoop loop;
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("DestinationFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, modification_time,
      base::BindLambdaForTesting([&loop](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kSuccess);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, std::string(""));

  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(file_path, &file_info));
  EXPECT_EQ(file_info.size, 0);

  // Because Mac rounds file modification time to the nearest second, make sure
  // the difference is within that range.
  base::TimeDelta difference = file_info.last_modified - modification_time;
  EXPECT_LT(difference.magnitude(), base::Seconds(1));

  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, InvalidInputFileSize) {
  SetUpOnDiskContext();
  static const std::string kData = "ABCDE";

  base::FilePath copy_from_file =
      temp_dir_.GetPath().AppendASCII("SourceFile.txt");

  // Create a file to copy from.
  base::Time modification_time =
      TruncateToSeconds(base::Time::Now() - base::Days(1));
  CreateFile(copy_from_file, kData, modification_time);

  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>("1234");
  builder->AppendFile(copy_from_file, 0ll, kData.size() * 2, modification_time);
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->AddFinishedBlob(std::move(builder));

  mojo::Remote<blink::mojom::Blob> blob;
  BlobImpl::Create(std::move(blob_handle), blob.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  base::RunLoop loop;
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("DestinationFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, modification_time,
      base::BindLambdaForTesting([&loop](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kInvalidBlob);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, InvalidInputFileTimeModified) {
  SetUpOnDiskContext();
  static const std::string kData = "ABCDE";

  base::FilePath copy_from_file =
      temp_dir_.GetPath().AppendASCII("SourceFile.txt");

  base::Time file_modified_time =
      TruncateToSeconds(base::Time::Now() - base::Days(1));
  CreateFile(copy_from_file, kData, file_modified_time);

  // Create the blob but give it the wrong modification time.
  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>("1234");
  base::Time bad_modified_time =
      TruncateToSeconds(base::Time::Now() - base::Days(2));
  builder->AppendFile(copy_from_file, 0ll, kData.size(), bad_modified_time);
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->AddFinishedBlob(std::move(builder));

  mojo::Remote<blink::mojom::Blob> blob;
  BlobImpl::Create(std::move(blob_handle), blob.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  base::RunLoop loop;
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("DestinationFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, std::nullopt,
      base::BindLambdaForTesting([&loop](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kInvalidBlob);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, NoProfileDirectory) {
  SetUpInMemoryContext();
  static const std::string kData = "ABCDE";

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();
  mojo::Remote<blink::mojom::Blob> blob;
  context->RegisterFromMemory(
      blob.BindNewPipeAndPassReceiver(), "1234",
      mojo_base::BigBuffer(base::as_bytes(base::make_span(kData))));

  base::RunLoop loop;
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("TestFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, std::nullopt,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kBadPath);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(BlobStorageContextMojoTest, PathWithReferences) {
  SetUpOnDiskContext();
  static const std::string kData = "ABCDE";

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();
  mojo::Remote<blink::mojom::Blob> blob;
  context->RegisterFromMemory(
      blob.BindNewPipeAndPassReceiver(), "1234",
      mojo_base::BigBuffer(base::as_bytes(base::make_span(kData))));

  base::RunLoop loop;
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("..").AppendASCII("UnaccessibleFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, std::nullopt,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kBadPath);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(BlobStorageContextMojoTest, InvalidPath) {
  SetUpOnDiskContext();
  static const std::string kData = "ABCDE";

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();
  mojo::Remote<blink::mojom::Blob> blob;
  context->RegisterFromMemory(
      blob.BindNewPipeAndPassReceiver(), "1234",
      mojo_base::BigBuffer(base::as_bytes(base::make_span(kData))));

  base::RunLoop loop;
  base::FilePath file_path = base::FilePath::FromUTF8Unsafe("/etc/passwd");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, std::nullopt,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kBadPath);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(BlobStorageContextMojoTest, SaveBlobToFileNoDirectory) {
  SetUpOnDiskContext();
  const std::string kData = "Hello There!";
  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  mojo::Remote<blink::mojom::Blob> blob;
  context->RegisterFromMemory(
      blob.BindNewPipeAndPassReceiver(), "1234",
      mojo_base::BigBuffer(base::as_bytes(base::make_span(kData))));

  // Create a 'last modified' that is different from now.
  base::Time last_modified =
      TruncateToSeconds(base::Time::Now() - base::Days(1));

  base::RunLoop loop;
  base::FilePath file_path = temp_dir_.GetPath()
                                 .AppendASCII("NotCreatedDirectory")
                                 .AppendASCII("TestFile.txt");
  context->WriteBlobToFile(
      blob.Unbind(), file_path, true, last_modified,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kIOError);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(file_path));
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, SaveOptimizedBlobToFileNoDirectory) {
  SetUpOnDiskContext();
  const std::string kData = "Hello There!";

  base::FilePath copy_from_file =
      temp_dir_.GetPath().AppendASCII("SourceFile.txt");

  // Create a file to copy from.
  CreateFile(copy_from_file, kData, std::nullopt);

  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>("1234");
  builder->AppendFile(copy_from_file, 0, kData.size(), base::Time());
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->AddFinishedBlob(std::move(builder));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  BlobImpl::Create(std::move(blob_handle),
                   blob.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  base::RunLoop loop;
  base::FilePath file_path = temp_dir_.GetPath()
                                 .AppendASCII("NotCreatedDirectory")
                                 .AppendASCII("TestFile.txt");
  context->WriteBlobToFile(
      std::move(blob), file_path, true, std::nullopt,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kIOError);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(file_path));
  ASSERT_TRUE(temp_dir_.Delete());
}

TEST_F(BlobStorageContextMojoTest, SaveOptimizedBlobNoFileSize) {
  SetUpOnDiskContext();
  const std::string kData = "Hello There!";

  base::FilePath copy_from_file =
      temp_dir_.GetPath().AppendASCII("SourceFile.txt");

  // Create a file to copy from.
  CreateFile(copy_from_file, kData, std::nullopt);

  std::unique_ptr<BlobDataBuilder> builder =
      std::make_unique<BlobDataBuilder>("1234");
  builder->AppendFile(copy_from_file, 0, blink::BlobUtils::kUnknownSize,
                      base::Time());
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->AddFinishedBlob(std::move(builder));

  mojo::PendingRemote<blink::mojom::Blob> blob;
  BlobImpl::Create(std::move(blob_handle),
                   blob.InitWithNewPipeAndPassReceiver());

  mojo::Remote<mojom::BlobStorageContext> context = CreateContextConnection();

  base::RunLoop loop;
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("TestFile.txt");
  context->WriteBlobToFile(
      std::move(blob), file_path, true, std::nullopt,
      base::BindLambdaForTesting([&](mojom::WriteBlobToFileResult result) {
        EXPECT_EQ(result, mojom::WriteBlobToFileResult::kSuccess);
        loop.Quit();
      }));
  loop.Run();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(file_path, &file_contents));
  EXPECT_EQ(file_contents, kData);

  base::DeleteFile(file_path);
  ASSERT_TRUE(temp_dir_.Delete());
}

}  // namespace
}  // namespace storage
