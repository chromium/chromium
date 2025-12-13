// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_decoding_interceptor.h"

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/net_errors.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/mock_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Test file names.
constexpr std::string_view kOriginalTestFile = "google.txt";
constexpr std::string_view kBrotliTestFile = "google.br";
constexpr std::string_view kZstdTestFile = "google.zst";
constexpr std::string_view kGzipTestFile = "google.gz";
constexpr std::string_view kGzipBrotliTestFile = "google.gz.br";

// Get the path of data directory.
base::FilePath GetTestDataDir() {
  base::FilePath data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir);
  return data_dir.AppendASCII("services")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("decoder");
}

// Reads the content of the specified test file.
std::string ReadTestData(std::string_view file_name) {
  std::string data;
  CHECK(base::ReadFileToString(GetTestDataDir().AppendASCII(file_name), &data));
  CHECK(!data.empty());
  return data;
}

// Creates data pipe with the specified capacity.
void CreatePipe(uint32_t capacity_num_bytes,
                mojo::ScopedDataPipeProducerHandle& data_pipe_producer,
                mojo::ScopedDataPipeConsumerHandle& data_pipe_consumer) {
  const MojoCreateDataPipeOptions options{
      .struct_size = sizeof(MojoCreateDataPipeOptions),
      .flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE,
      .element_num_bytes = 1,
      .capacity_num_bytes = capacity_num_bytes};
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&options, data_pipe_producer,
                                                 data_pipe_consumer));
}

// Creates a data pipe pair using the CreateDataPipePair method of
// ContentDecodingInterceptor.
ContentDecodingInterceptor::DataPipePair CreateDataPipePair() {
  auto result = ContentDecodingInterceptor::CreateDataPipePair(
      ContentDecodingInterceptor::ClientType::kTest);
  CHECK(result.has_value());
  return std::move(*result);
}

// Reads the content of the given test file, writes it to a new data pipe, and
// returns the consumer handle for that pipe.
mojo::ScopedDataPipeConsumerHandle ReadFileAndWriteToNewPipe(
    std::string_view file_name) {
  const std::string test_data = ReadTestData(file_name);
  mojo::ScopedDataPipeProducerHandle source_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreatePipe(test_data.size(), source_producer, consumer);

  // Write the data.
  CHECK_EQ(source_producer->WriteAllData(base::as_byte_span(test_data)),
           MOJO_RESULT_OK);
  // Finish the data by closing the producer.
  source_producer.reset();
  return consumer;
}

// Reads data from a data pipe using DataPipeDrainer.
class DataPipeReader : public mojo::DataPipeDrainer::Client {
 public:
  DataPipeReader(std::string* data_out, base::OnceClosure done_callback)
      : data_out_(data_out), done_callback_(std::move(done_callback)) {}

  // mojo::DataPipeDrainer::Client implementation:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    data_out_->append(base::as_string_view(data));
  }

  void OnDataComplete() override { std::move(done_callback_).Run(); }

 private:
  raw_ptr<std::string> data_out_;
  base::OnceClosure done_callback_;
};

// Reads data from a data pipe to a string.
std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle pipe) {
  base::RunLoop loop;
  std::string data;
  DataPipeReader reader(&data, loop.QuitClosure());
  mojo::DataPipeDrainer drainer(&reader, std::move(pipe));
  loop.Run();
  return data;
}

// Mock implementation of network::mojom::URLLoader for testing.
class MockURLLoader : public network::mojom::URLLoader {
 public:
  MockURLLoader() = default;
  MockURLLoader(const MockURLLoader&) = delete;
  MockURLLoader& operator=(const MockURLLoader&) = delete;
  ~MockURLLoader() override = default;

  // network::mojom::URLLoader implementation:
  MOCK_METHOD(void,
              FollowRedirect,
              (const std::vector<std::string>& removed_headers,
               const net::HttpRequestHeaders& modified_headers,
               const net::HttpRequestHeaders& modified_cors_exempt_headers,
               const std::optional<GURL>& new_url),
              (override));
  MOCK_METHOD(void,
              SetPriority,
              (net::RequestPriority priority, int32_t intra_priority_value),
              (override));
};

// Performs a simple decoding test. Reads data from the specified test file,
// decodes it using the provided `types`, and compares the result with the
// expected original data.
void TestSimpleDecodeTest(const std::string_view file_name,
                          const std::vector<net::SourceStreamType>& types) {
  const std::string test_data = ReadTestData(file_name);
  const std::string expected_data = ReadTestData(kOriginalTestFile);

  mojo::ScopedDataPipeProducerHandle source_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreatePipe(test_data.size(), source_producer, consumer);

  mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver;
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_remote;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New(
      url_loader_receiver.InitWithNewPipeAndPassRemote(),
      url_loader_client_remote.BindNewPipeAndPassReceiver());

  ContentDecodingInterceptor::Intercept(
      types, endpoints, consumer, CreateDataPipePair(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}));

  // Write the encoded data.
  ASSERT_EQ(source_producer->WriteAllData(base::as_byte_span(test_data)),
            MOJO_RESULT_OK);
  // Finish the data by closing the producer.
  source_producer.reset();
  // Send OnComplete(). The interceptor calls
  // MockURLLoaderClient::OnComplete() after finishing decoding.
  url_loader_client_remote->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));

  EXPECT_EQ(ReadDataPipe(std::move(consumer)), expected_data);

  base::RunLoop run_loop;
  testing::NiceMock<network::MockURLLoaderClient> client;
  EXPECT_CALL(client, OnComplete)
      .WillOnce([&](::network::URLLoaderCompletionStatus st) {
        EXPECT_EQ(st.error_code, net::OK);
        EXPECT_EQ(st.decoded_body_length,
                  base::checked_cast<int64_t>(expected_data.size()));
        run_loop.Quit();
      });
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(
      &client, std::move(endpoints->url_loader_client));
  run_loop.Run();
}

}  // namespace

// Test fixture for ContentDecodingInterceptor tests.
class ContentDecodingInterceptorTest : public testing::Test {
 public:
  ContentDecodingInterceptorTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~ContentDecodingInterceptorTest() override = default;

 protected:
  // Provides a task environment for the tests.
  base::test::TaskEnvironment task_environment_;
};

// Tests decoding of Brotli-encoded data.
TEST_F(ContentDecodingInterceptorTest, SimpleBrotli) {
  TestSimpleDecodeTest(kBrotliTestFile, {net::SourceStreamType::kBrotli});
}

// Tests decoding of Zstd-encoded data.
TEST_F(ContentDecodingInterceptorTest, SimpleZstd) {
  TestSimpleDecodeTest(kZstdTestFile, {net::SourceStreamType::kZstd});
}

// Tests decoding of Gzip-encoded data.
TEST_F(ContentDecodingInterceptorTest, SimpleGzip) {
  TestSimpleDecodeTest(kGzipTestFile, {net::SourceStreamType::kGzip});
}

// Tests decoding of data encoded with Gzip followed by Brotli.
TEST_F(ContentDecodingInterceptorTest, SimpleGzipBrotli) {
  TestSimpleDecodeTest(kGzipBrotliTestFile, {net::SourceStreamType::kGzip,
                                             net::SourceStreamType::kBrotli});
}

// Tests the case when OnComplete is called before the decoding finishes.
TEST_F(ContentDecodingInterceptorTest, OnCompleteBeforeOnFinishDecode) {
  const std::string_view file_name = kBrotliTestFile;
  const std::vector<net::SourceStreamType> types = {
      net::SourceStreamType::kBrotli};

  const std::string test_data = ReadTestData(file_name);
  const std::string expected_data = ReadTestData(kOriginalTestFile);

  mojo::ScopedDataPipeProducerHandle source_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreatePipe(test_data.size(), source_producer, consumer);

  mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver;
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_remote;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New(
      url_loader_receiver.InitWithNewPipeAndPassRemote(),
      url_loader_client_remote.BindNewPipeAndPassReceiver());

  auto worker_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING});

  ContentDecodingInterceptor::Intercept(
      types, endpoints, consumer, CreateDataPipePair(), worker_task_runner);

  // Send OnComplete() IPC.
  url_loader_client_remote->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));

  // Post a QuitClosure task to make sure the InterceptorImpl receives the
  // OnComplete() IPC.
  {
    base::RunLoop run_loop;
    worker_task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  };

  // And then send data and finish the data.
  ASSERT_EQ(source_producer->WriteAllData(base::as_byte_span(test_data)),
            MOJO_RESULT_OK);
  source_producer.reset();

  EXPECT_EQ(ReadDataPipe(std::move(consumer)), expected_data);

  base::RunLoop run_loop;
  testing::NiceMock<network::MockURLLoaderClient> client;
  EXPECT_CALL(client, OnComplete)
      .WillOnce([&](::network::URLLoaderCompletionStatus st) {
        EXPECT_EQ(st.error_code, net::OK);
        EXPECT_EQ(st.decoded_body_length,
                  base::checked_cast<int64_t>(expected_data.size()));
        run_loop.Quit();
      });
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(
      &client, std::move(endpoints->url_loader_client));
  run_loop.Run();
}

// Tests the case where the specified content type is incorrect, resulting in a
// decoding error.
TEST_F(ContentDecodingInterceptorTest, WrongContentType) {
  const std::string_view file_name = kGzipTestFile;
  const std::vector<net::SourceStreamType> types = {
      net::SourceStreamType::kBrotli};

  const std::string test_data = ReadTestData(file_name);

  mojo::ScopedDataPipeProducerHandle source_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreatePipe(test_data.size(), source_producer, consumer);

  mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver;
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_remote;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New(
      url_loader_receiver.InitWithNewPipeAndPassRemote(),
      url_loader_client_remote.BindNewPipeAndPassReceiver());

  ContentDecodingInterceptor::Intercept(
      types, endpoints, consumer, CreateDataPipePair(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}));

  // Write the wrong encoded data.
  ASSERT_EQ(source_producer->WriteAllData(base::as_byte_span(test_data)),
            MOJO_RESULT_OK);
  // Finish the data.
  source_producer.reset();
  // Send OnComplete.
  url_loader_client_remote->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));

  // The response body should be empty.
  EXPECT_TRUE(ReadDataPipe(std::move(consumer)).empty());

  base::RunLoop run_loop;
  testing::NiceMock<network::MockURLLoaderClient> client;
  EXPECT_CALL(client, OnComplete)
      .WillOnce([&](::network::URLLoaderCompletionStatus st) {
        // OnComplete must be called with ERR_CONTENT_DECODING_FAILED.
        EXPECT_EQ(st.error_code, net::ERR_CONTENT_DECODING_FAILED);
        EXPECT_EQ(st.decoded_body_length, 0u);
        run_loop.Quit();
      });
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(
      &client, std::move(endpoints->url_loader_client));
  run_loop.Run();
}

// Tests the case where an error occurred in the network.
TEST_F(ContentDecodingInterceptorTest, UrlLoaderError) {
  const std::string_view file_name = kBrotliTestFile;
  const std::vector<net::SourceStreamType> types = {
      net::SourceStreamType::kBrotli};

  const std::string test_data = ReadTestData(file_name);
  const std::string expected_data = ReadTestData(kOriginalTestFile);

  mojo::ScopedDataPipeProducerHandle source_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreatePipe(test_data.size(), source_producer, consumer);

  mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver;
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_remote;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New(
      url_loader_receiver.InitWithNewPipeAndPassRemote(),
      url_loader_client_remote.BindNewPipeAndPassReceiver());

  ContentDecodingInterceptor::Intercept(
      types, endpoints, consumer, CreateDataPipePair(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}));

  // Write the encoded data.
  ASSERT_EQ(source_producer->WriteAllData(base::as_byte_span(test_data)),
            MOJO_RESULT_OK);
  // Finish the data.
  source_producer.reset();

  // Read the decoded data.
  EXPECT_EQ(ReadDataPipe(std::move(consumer)), expected_data);

  // Send OnComplete with ERR_FAILED.
  url_loader_client_remote->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  base::RunLoop run_loop;
  testing::NiceMock<network::MockURLLoaderClient> client;
  EXPECT_CALL(client, OnComplete)
      .WillOnce([&](::network::URLLoaderCompletionStatus st) {
        // OnComplete must be caled with ERR_FAILED.
        EXPECT_EQ(st.error_code, net::ERR_FAILED);
        EXPECT_EQ(st.decoded_body_length, 0u);
        run_loop.Quit();
      });
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(
      &client, std::move(endpoints->url_loader_client));
  run_loop.Run();
}

// Verifies that the interceptor correctly forwards SetPriority() calls
// and that decoding still functions as expected.
TEST_F(ContentDecodingInterceptorTest, SetPriority) {
  const std::string_view file_name = kBrotliTestFile;
  const std::vector<net::SourceStreamType> types = {
      net::SourceStreamType::kBrotli};
  const std::string test_data = ReadTestData(file_name);
  const std::string expected_data = ReadTestData(kOriginalTestFile);

  mojo::ScopedDataPipeProducerHandle source_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreatePipe(test_data.size(), source_producer, consumer);

  mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver;
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_remote;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New(
      url_loader_receiver.InitWithNewPipeAndPassRemote(),
      url_loader_client_remote.BindNewPipeAndPassReceiver());

  ContentDecodingInterceptor::Intercept(
      types, endpoints, consumer, CreateDataPipePair(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}));

  mojo::Remote<network::mojom::URLLoader> loader_remote(
      std::move(endpoints->url_loader));
  {
    base::RunLoop run_loop;
    testing::NiceMock<network::MockURLLoader> loader;
    // Expect SetPriority() to be called on the mock URLLoader with the
    // specified priority and intra-priority value.
    EXPECT_CALL(loader, SetPriority)
        .WillOnce(
            [&](net::RequestPriority priority, int32_t intra_priority_value) {
              EXPECT_EQ(priority, net::HIGHEST);
              EXPECT_EQ(intra_priority_value, 10);
              run_loop.Quit();
            });
    mojo::Receiver<network::mojom::URLLoader> loader_receiver(
        &loader, std::move(url_loader_receiver));

    // Call SetPriority() on the intercepted URLLoader. This should be
    // forwarded to the mock URLLoader.
    loader_remote->SetPriority(net::HIGHEST, 10);
    run_loop.Run();
    url_loader_receiver = loader_receiver.Unbind();
  }

  // The following code does the same as TestSimpleDecodeTest() to check that
  // the interceptor works as expected after SetPriority() is called.

  // Write the encoded data.
  ASSERT_EQ(source_producer->WriteAllData(base::as_byte_span(test_data)),
            MOJO_RESULT_OK);
  // Finish the data.
  source_producer.reset();
  // Send OnComplete.
  url_loader_client_remote->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));

  EXPECT_EQ(ReadDataPipe(std::move(consumer)), expected_data);

  base::RunLoop run_loop;
  testing::NiceMock<network::MockURLLoaderClient> client;
  EXPECT_CALL(client, OnComplete)
      .WillOnce([&](network::URLLoaderCompletionStatus st) {
        EXPECT_EQ(st.error_code, net::OK);
        EXPECT_EQ(st.decoded_body_length,
                  base::checked_cast<int64_t>(expected_data.size()));
        run_loop.Quit();
      });
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(
      &client, std::move(endpoints->url_loader_client));
  run_loop.Run();
}

// Verifies that the interceptor correctly forwards OnTransferSizeUpdated()
// calls and that decoding still functions as expected.
TEST_F(ContentDecodingInterceptorTest, OnTransferSizeUpdated) {
  const std::string_view file_name = kBrotliTestFile;
  const std::vector<net::SourceStreamType> types = {
      net::SourceStreamType::kBrotli};
  const std::string test_data = ReadTestData(file_name);
  const std::string expected_data = ReadTestData(kOriginalTestFile);

  mojo::ScopedDataPipeProducerHandle source_producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreatePipe(test_data.size(), source_producer, consumer);

  mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver;
  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_remote;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New(
      url_loader_receiver.InitWithNewPipeAndPassRemote(),
      url_loader_client_remote.BindNewPipeAndPassReceiver());

  ContentDecodingInterceptor::Intercept(
      types, endpoints, consumer, CreateDataPipePair(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}));

  // Send OnTransferSizeUpdated IPC.
  url_loader_client_remote->OnTransferSizeUpdated(123);

  // Write the encoded data.
  ASSERT_EQ(source_producer->WriteAllData(base::as_byte_span(test_data)),
            MOJO_RESULT_OK);
  // Finish the data.
  source_producer.reset();
  // Send OnComplete.
  url_loader_client_remote->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));

  EXPECT_EQ(ReadDataPipe(std::move(consumer)), expected_data);

  base::RunLoop run_loop;
  testing::NiceMock<network::MockURLLoaderClient> client;
  // Expect OnTransferSizeUpdated() to be called on the mock client with the
  // specified transfer size difference.
  EXPECT_CALL(client, OnTransferSizeUpdated)
      .WillOnce([](int32_t transfer_size_diff) {
        EXPECT_EQ(transfer_size_diff, 123);
      });
  EXPECT_CALL(client, OnComplete)
      .WillOnce([&](network::URLLoaderCompletionStatus st) {
        EXPECT_EQ(st.error_code, net::OK);
        EXPECT_EQ(st.decoded_body_length,
                  base::checked_cast<int64_t>(expected_data.size()));
        run_loop.Quit();
      });
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(
      &client, std::move(endpoints->url_loader_client));
  run_loop.Run();
}

// Verifies the behavior when the interceptor fails to create its internal Mojo
// data pipe, simulating a resource exhaustion scenario.
TEST_F(ContentDecodingInterceptorTest, CreateDataPipeFailure) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{network::features::kRendererSideContentDecoding,
        {{"RendererSideContentDecodingForceMojoFailureForTesting", "true"}}}},
      {});
  auto data_pipe_pair = ContentDecodingInterceptor::CreateDataPipePair(
      ContentDecodingInterceptor::ClientType::kTest);
  EXPECT_FALSE(data_pipe_pair.has_value());
}

// Tests the successful decoding of a Brotli-compressed stream using
// DecodeOnNetworkService.
TEST_F(ContentDecodingInterceptorTest, DecodeOnNetworkService) {
  auto body = ReadFileAndWriteToNewPipe(kBrotliTestFile);
  auto network_service = NetworkService::CreateForTesting();

  base::test::TestFuture<net::Error> future;
  ContentDecodingInterceptor::DecodeOnNetworkService(
      *network_service, {net::SourceStreamType::kBrotli}, body,
      ContentDecodingInterceptor::ClientType::kTest, future.GetCallback());
  // Wait for the decoding to finish. The TestFuture will be set to net::OK
  // when the decoding is successful.
  EXPECT_EQ(future.Get(), net::OK);
  // Read the decoded output from the data pipe.
  EXPECT_EQ(ReadDataPipe(std::move(body)), ReadTestData(kOriginalTestFile));
}

// Tests decoding on the network service with an incorrect content encoding
// type. This should result in a decoding failure.
TEST_F(ContentDecodingInterceptorTest, DecodeOnNetworkServiceWrongType) {
  auto body = ReadFileAndWriteToNewPipe(kBrotliTestFile);
  auto network_service = NetworkService::CreateForTesting();

  base::test::TestFuture<net::Error> future;
  ContentDecodingInterceptor::DecodeOnNetworkService(
      *network_service, {net::SourceStreamType::kZstd}, body,
      ContentDecodingInterceptor::ClientType::kTest, future.GetCallback());
  // Expect the decoding to fail since the content type is wrong.
  EXPECT_EQ(future.Get(), net::ERR_CONTENT_DECODING_FAILED);
  // There should be no data in the `body`.
  EXPECT_TRUE(ReadDataPipe(std::move(body)).empty());
}

// Tests decoding on the network service but simulating a failure to create a
// data pipe, which can happen due to resource exhaustion. The decoding should
// fail in this scenario.
TEST_F(ContentDecodingInterceptorTest,
       DecodeOnNetworkServiceCreateDataPipeFailure) {
  auto body = ReadFileAndWriteToNewPipe(kBrotliTestFile);
  auto network_service = NetworkService::CreateForTesting();

  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{network::features::kRendererSideContentDecoding,
        {{"RendererSideContentDecodingForceMojoFailureForTesting", "true"}}}},
      {});
  base::test::TestFuture<net::Error> future;
  ContentDecodingInterceptor::DecodeOnNetworkService(
      *network_service, {net::SourceStreamType::kBrotli}, body,
      ContentDecodingInterceptor::ClientType::kTest, future.GetCallback());
  // Expect the decoding to fail since the datapipe could not be created.
  EXPECT_EQ(future.Get(), net::ERR_INSUFFICIENT_RESOURCES);

  // Even if the data pipe creation failed, the data in the original pipe
  // should remain unchanged. Verify that the data in the original pipe `body`
  // is still the brotli encoded data.
  EXPECT_EQ(ReadDataPipe(std::move(body)), ReadTestData(kBrotliTestFile));
}

// Tests decoding on the network service, but simulating a disconnect from the
// network service. The decoding should fail in this scenario since the
// decoding depends on the network service.
TEST_F(ContentDecodingInterceptorTest, DecodeOnNetworkServiceDisconnected) {
  auto body = ReadFileAndWriteToNewPipe(kBrotliTestFile);

  mojo::Remote<mojom::NetworkService> network_service_remote;
  mojo::PendingReceiver<mojom::NetworkService> network_service_receiver =
      network_service_remote.BindNewPipeAndPassReceiver();

  base::test::TestFuture<net::Error> future;
  ContentDecodingInterceptor::DecodeOnNetworkService(
      *network_service_remote, {net::SourceStreamType::kBrotli}, body,
      ContentDecodingInterceptor::ClientType::kTest, future.GetCallback());
  network_service_receiver.reset();
  // Expect the decoding to fail because the network service is disconnected
  // before the decoding completes.
  EXPECT_EQ(future.Get(), net::ERR_FAILED);
  // There should be no data in the `body`.
  EXPECT_TRUE(ReadDataPipe(std::move(body)).empty());
}

}  // namespace network
