// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_blob_data_source.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr size_t kTestBlobStorageMaxBytesDataItemSize = 13;
constexpr size_t kTestBlobStorageMaxBlobMemorySize = 500;
constexpr uint64_t kTestBlobStorageMinFileSizeBytes = 32;
constexpr uint64_t kTestBlobStorageMaxFileSizeBytes = 100;
constexpr uint64_t kTestBlobStorageMaxDiskSpace = 1000;

}  // namespace

class BundledExchangesBlobDataSourceTest : public testing::Test {
 protected:
  BundledExchangesBlobDataSourceTest() = default;
  ~BundledExchangesBlobDataSourceTest() override = default;
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_ = std::make_unique<storage::BlobStorageContext>(
        data_dir_.GetPath(),
        base::CreateTaskRunner({base::ThreadPool(), base::MayBlock()}));
    storage::BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageMaxBytesDataItemSize;
    limits.max_shared_memory_size = kTestBlobStorageMaxBytesDataItemSize;
    limits.max_bytes_data_item_size = kTestBlobStorageMaxBytesDataItemSize;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    limits.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    context_->set_limits_for_testing(limits);
  }

  std::unique_ptr<BundledExchangesBlobDataSource> CreateTestDataSource(
      const std::string& test_data,
      mojo::Remote<data_decoder::mojom::BundleDataSource>* remote_source,
      base::Optional<int64_t> content_length = base::nullopt) {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    CHECK_EQ(MOJO_RESULT_OK,
             mojo::CreateDataPipe(nullptr, &producer, &consumer));
    mojo::BlockingCopyFromString(test_data, producer);
    producer.reset();

    return std::make_unique<BundledExchangesBlobDataSource>(
        content_length ? *content_length : test_data.size(),
        std::move(consumer), nullptr, ContextGetter(),
        remote_source->BindNewPipeAndPassReceiver());
  }
  std::unique_ptr<storage::BlobStorageContext> context_;

 private:
  BrowserContext::BlobContextGetter ContextGetter() {
    return base::BindRepeating(
        [](base::WeakPtr<storage::BlobStorageContext> weak_context) {
          return weak_context;
        },
        context_->AsWeakPtr());
  }

  base::ScopedTempDir data_dir_;
  BrowserTaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(BundledExchangesBlobDataSourceTest);
};

TEST_F(BundledExchangesBlobDataSourceTest, GetSize) {
  const std::string kData = "Test Data";
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(kData, &remote_source);

  base::RunLoop run_loop;
  uint64_t size_result = 0;
  remote_source->GetSize(base::BindOnce(
      [](base::OnceClosure closure, uint64_t* size_result, uint64_t size) {
        *size_result = size;
        std::move(closure).Run();
      },
      run_loop.QuitClosure(), &size_result));
  run_loop.Run();
  EXPECT_EQ(kData.size(), size_result);
}

TEST_F(BundledExchangesBlobDataSourceTest, Read) {
  const std::string kData = "Test Data";
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(kData, &remote_source);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;
  remote_source->Read(
      1, 3,
      base::BindOnce(
          [](base::OnceClosure closure,
             base::Optional<std::vector<uint8_t>>* read_result,
             const base::Optional<std::vector<uint8_t>>& result) {
            *read_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_result));
  run_loop.Run();
  ASSERT_TRUE(read_result);
  ASSERT_EQ(3u, read_result->size());
  EXPECT_EQ('e', (*read_result)[0]);
  EXPECT_EQ('s', (*read_result)[1]);
  EXPECT_EQ('t', (*read_result)[2]);
}

TEST_F(BundledExchangesBlobDataSourceTest, Read_OutOfRangeError) {
  const std::string kData = "Test Data";
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(kData, &remote_source);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;
  remote_source->Read(
      1, 100,
      base::BindOnce(
          [](base::OnceClosure closure,
             base::Optional<std::vector<uint8_t>>* read_result,
             const base::Optional<std::vector<uint8_t>>& result) {
            *read_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_result));
  run_loop.Run();
  EXPECT_FALSE(read_result);
}

TEST_F(BundledExchangesBlobDataSourceTest, Read_InvalidContentLengthError) {
  const std::string kData = "Test Data";
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(kData, &remote_source, kData.size() + 1);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;
  remote_source->Read(
      1, 3,
      base::BindOnce(
          [](base::OnceClosure closure,
             base::Optional<std::vector<uint8_t>>* read_result,
             const base::Optional<std::vector<uint8_t>>& result) {
            *read_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_result));
  run_loop.Run();
  EXPECT_FALSE(read_result);
}

TEST_F(BundledExchangesBlobDataSourceTest, Read_NoStorage) {
  std::string content = "Test Data";
  // Make the content larger than the disk space.
  content.resize(kTestBlobStorageMaxDiskSpace + 1, ' ');
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(content, &remote_source);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;
  remote_source->Read(
      1, 100,
      base::BindOnce(
          [](base::OnceClosure closure,
             base::Optional<std::vector<uint8_t>>* read_result,
             const base::Optional<std::vector<uint8_t>>& result) {
            *read_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_result));
  run_loop.Run();
  EXPECT_FALSE(read_result);
}

TEST_F(BundledExchangesBlobDataSourceTest, ReadToDataPipe) {
  const std::string kData = "Test Data";
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(kData, &remote_source);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &producer, &consumer));

  net::Error read_response_body_result = net::ERR_FAILED;
  source->ReadToDataPipe(
      1, 3, std::move(producer),
      base::BindOnce(
          [](base::OnceClosure closure, net::Error* read_response_body_result,
             net::Error result) {
            *read_response_body_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_response_body_result));
  run_loop.Run();
  EXPECT_EQ(net::OK, read_response_body_result);

  std::string result_string;
  mojo::BlockingCopyToString(std::move(consumer), &result_string);
  EXPECT_EQ(3u, result_string.size());
  EXPECT_EQ(kData.substr(1, 3), result_string);
}

TEST_F(BundledExchangesBlobDataSourceTest, ReadToDataPipe_OutOfRangeError) {
  const std::string kData = "Test Data";
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(kData, &remote_source);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &producer, &consumer));

  net::Error read_response_body_result = net::ERR_FAILED;
  source->ReadToDataPipe(
      1, 100, std::move(producer),
      base::BindOnce(
          [](base::OnceClosure closure, net::Error* read_response_body_result,
             net::Error result) {
            *read_response_body_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_response_body_result));
  run_loop.Run();
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, read_response_body_result);
}

TEST_F(BundledExchangesBlobDataSourceTest,
       ReadToDataPipe_InvalidContentLengthError) {
  const std::string kData = "Test Data";
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(kData, &remote_source, kData.size() + 1);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &producer, &consumer));

  net::Error read_response_body_result = net::OK;
  source->ReadToDataPipe(
      1, 3, std::move(producer),
      base::BindOnce(
          [](base::OnceClosure closure, net::Error* read_response_body_result,
             net::Error result) {
            *read_response_body_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_response_body_result));
  run_loop.Run();
  EXPECT_EQ(net::ERR_FAILED, read_response_body_result);
}

TEST_F(BundledExchangesBlobDataSourceTest, ReadToDataPipe_NoStorage) {
  std::string content = "Test Data";
  // Make the content larger than the disk space.
  content.resize(kTestBlobStorageMaxDiskSpace + 1, ' ');
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(content, &remote_source);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &producer, &consumer));

  net::Error read_response_body_result = net::OK;
  source->ReadToDataPipe(
      1, 3, std::move(producer),
      base::BindOnce(
          [](base::OnceClosure closure, net::Error* read_response_body_result,
             net::Error result) {
            *read_response_body_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_response_body_result));
  run_loop.Run();
  EXPECT_EQ(net::ERR_FAILED, read_response_body_result);
}

TEST_F(BundledExchangesBlobDataSourceTest, ReadToDataPipe_Destructed) {
  const std::string kData = "Test Data";
  mojo::Remote<data_decoder::mojom::BundleDataSource> remote_source;
  auto source = CreateTestDataSource(kData, &remote_source);

  base::RunLoop run_loop;
  base::Optional<std::vector<uint8_t>> read_result;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &producer, &consumer));

  net::Error read_response_body_result = net::OK;
  source->ReadToDataPipe(
      1, 3, std::move(producer),
      base::BindOnce(
          [](base::OnceClosure closure, net::Error* read_response_body_result,
             net::Error result) {
            *read_response_body_result = result;
            std::move(closure).Run();
          },
          run_loop.QuitClosure(), &read_response_body_result));
  source.reset();
  run_loop.Run();
  EXPECT_EQ(net::ERR_FAILED, read_response_body_result);
}

}  // namespace content
