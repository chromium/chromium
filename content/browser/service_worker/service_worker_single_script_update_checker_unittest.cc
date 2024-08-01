// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_single_script_update_checker.h"

#include <vector>

#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

constexpr char kScriptURL[] = "https://example.com/script.js";
constexpr char kImportedScriptURL[] = "https://example.com/imported-script.js";
constexpr char kScope[] = "https://example.com/";
constexpr char kSuccessHeader[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/javascript\n\n";

class ServiceWorkerSingleScriptUpdateCheckerTest : public testing::Test {
 public:
  struct CheckResult {
    CheckResult(
        const GURL& script_url,
        ServiceWorkerSingleScriptUpdateChecker::Result compare_result,
        std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
            failure_info,
        std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
            paused_state,
        const std::optional<std::string>& sha256_checksum)
        : url(script_url),
          result(compare_result),
          failure_info(std::move(failure_info)),
          paused_state(std::move(paused_state)),
          sha256_checksum(sha256_checksum) {}

    CheckResult(CheckResult&& ref) = default;

    CheckResult& operator=(CheckResult&& ref) = default;

    ~CheckResult() = default;

    GURL url;
    ServiceWorkerSingleScriptUpdateChecker::Result result;
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
        failure_info;
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state;
    std::optional<std::string> sha256_checksum;
  };

  ServiceWorkerSingleScriptUpdateCheckerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        browser_context_(std::make_unique<TestBrowserContext>()) {
    // Ensure the BrowserContext's storage partition is initialized.
    browser_context_->GetDefaultStoragePartition();
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerSingleScriptUpdateCheckerTest(
      const ServiceWorkerSingleScriptUpdateCheckerTest&) = delete;
  ServiceWorkerSingleScriptUpdateCheckerTest& operator=(
      const ServiceWorkerSingleScriptUpdateCheckerTest&) = delete;

  ~ServiceWorkerSingleScriptUpdateCheckerTest() override = default;

  size_t TotalBytes(const std::vector<std::string>& data_chunks) {
    size_t bytes = 0;
    for (const auto& data : data_chunks)
      bytes += data.size();
    return bytes;
  }

  // Create an update checker which will always ask HTTP cache validation.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker>
  CreateSingleScriptUpdateCheckerWithoutHttpCache(
      const char* url,
      const GURL& scope,
      std::unique_ptr<MockServiceWorkerResourceReader> compare_reader,
      std::unique_ptr<MockServiceWorkerResourceReader> copy_reader,
      std::unique_ptr<MockServiceWorkerResourceWriter> writer,
      network::TestURLLoaderFactory* loader_factory,
      std::optional<CheckResult>* out_check_result) {
    return CreateSingleScriptUpdateChecker(
        url, url, scope, false /* force_bypass_cache */,
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kNone,
        base::TimeDelta() /* time_since_last_check */,
        std::move(compare_reader), std::move(copy_reader), std::move(writer),
        loader_factory,
        ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
            kDefault,
        out_check_result);
  }

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> WrapReader(
      std::unique_ptr<MockServiceWorkerResourceReader> reader) {
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> remote;
    MockServiceWorkerResourceReader* raw_reader = reader.get();
    remote.Bind(raw_reader->BindNewPipeAndPassRemote(base::BindOnce(
        [](std::unique_ptr<MockServiceWorkerResourceReader>) {
          // Keep |reader| until mojo connection is destroyed.
        },
        std::move(reader))));
    return remote;
  }

  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> WrapWriter(
      std::unique_ptr<MockServiceWorkerResourceWriter> writer) {
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> remote;
    MockServiceWorkerResourceWriter* raw_writer = writer.get();
    remote.Bind(raw_writer->BindNewPipeAndPassRemote(base::BindOnce(
        [](std::unique_ptr<MockServiceWorkerResourceWriter>) {
          // Keep |writer| until mojo connection is destroyed.
        },
        std::move(writer))));
    return remote;
  }

  // Note that |loader_factory| should be alive as long as the single script
  // update checker is running.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker>
  CreateSingleScriptUpdateChecker(
      const char* url,
      const char* main_script_url,
      const GURL& scope,
      bool force_bypass_cache,
      blink::mojom::ScriptType worker_script_type,
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
      base::TimeDelta time_since_last_check,
      std::unique_ptr<MockServiceWorkerResourceReader> compare_reader,
      std::unique_ptr<MockServiceWorkerResourceReader> copy_reader,
      std::unique_ptr<MockServiceWorkerResourceWriter> writer,
      network::TestURLLoaderFactory* loader_factory,
      ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption
          script_checksum_update_option,
      std::optional<CheckResult>* out_check_result) {
    auto fetch_client_settings_object =
        blink::mojom::FetchClientSettingsObject::New(
            network::mojom::ReferrerPolicy::kDefault, GURL(main_script_url),
            blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade);
    return std::make_unique<ServiceWorkerSingleScriptUpdateChecker>(
        GURL(url), url == main_script_url, GURL(main_script_url), scope,
        force_bypass_cache, worker_script_type, update_via_cache,
        std::move(fetch_client_settings_object), time_since_last_check,
        browser_context_.get(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            loader_factory),
        WrapReader(std::move(compare_reader)),
        WrapReader(std::move(copy_reader)), WrapWriter(std::move(writer)),
        /*writer_resource_id=*/0, script_checksum_update_option,
        blink::StorageKey::Create(url::Origin::Create(scope),
                                  net::SchemefulSite(scope),
                                  blink::mojom::AncestorChainBit::kSameSite,
                                  /*third_party_partitioning_allowed=*/true),
        base::BindOnce(
            [](std::optional<CheckResult>* out_check_result_param,
               const GURL& script_url,
               ServiceWorkerSingleScriptUpdateChecker::Result result,
               std::unique_ptr<
                   ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
                   failure_info,
               std::unique_ptr<
                   ServiceWorkerSingleScriptUpdateChecker::PausedState>
                   paused_state,
               const std::optional<std::string>& sha256_checksum) {
              *out_check_result_param =
                  CheckResult(script_url, result, std::move(failure_info),
                              std::move(paused_state), sha256_checksum);
            },
            out_check_result));
  }

  std::unique_ptr<network::TestURLLoaderFactory> CreateLoaderFactoryWithRespone(
      const GURL& url,
      const std::string& header,
      const std::string& body,
      net::Error error) {
    auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(header));
    head->headers->GetMimeType(&head->mime_type);
    head->parsed_headers = network::mojom::ParsedHeaders::New();
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = body.size();
    loader_factory->AddResponse(url, std::move(head), body, status);
    return loader_factory;
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
};

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Identical_SingleRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{body_from_net};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body.
  compare_reader_rawptr->CompletePendingRead();

  // Complete the comparison of the body. It should be identical.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Identical_MultipleRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{"abc", "def"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body ("abc").
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body ("def").
  compare_reader_rawptr->CompletePendingRead();

  // Complete the comparison of the body. It should be identical.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Identical_Empty) {
  // Response body from the network, which is empty.
  const std::string body_from_net("");

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{body_from_net};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header. The initial block of the network body is empty, and
  // the empty body is passed to the cache writer. It will finish the
  // comparison immediately.
  compare_reader_rawptr->CompletePendingRead();

  // Both network and storage are empty. The result should be kIdentical.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_FALSE(check_result.value().paused_state);
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       Different_SingleRead_NetworkIsLonger) {
  // Response body from the network.
  const std::string body_from_net = "abcdef";

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{"abc", ""};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body ("abc").
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage (""). The cache writer detects the end of
  // the body from the disk cache.
  compare_reader_rawptr->CompletePendingRead();

  // Complete the comparison of the body. It should be different.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       Different_SingleRead_StorageIsLonger) {
  // Response body from the network.
  const std::string body_from_net = "abc";

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{"abc", "def"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body ("abc"). At this point, data from the network reaches
  // the end.
  compare_reader_rawptr->CompletePendingRead();

  // Complete the comparison of the body. It should be different.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);

  // The update checker realizes that the script is different before reaching
  // the end of the script from the disk cache.
  EXPECT_FALSE(compare_reader_rawptr->AllExpectedReadsDone());
}

// Regression test for https://crbug.com/995146.
// It should detect the update appropriately even when OnComplete() arrives
// after the end of the body.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       Different_SingleRead_EndOfTheBodyFirst) {
  // Response body from the network.
  const std::string body_from_net = "abc";

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{"abc", "def"};

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Wait until the testing loader factory receives the network request.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, loader_factory->pending_requests()->size());

  // |client| simulates sending the data from the network to the update checker.
  mojo::Remote<network::mojom::URLLoaderClient> client =
      std::move(loader_factory->GetPendingRequest(0)->client);

  // Simulate sending the response head.
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(kSuccessHeader));
  head->headers->GetMimeType(&head->mime_type);
  head->parsed_headers = network::mojom::ParsedHeaders::New();

  // Simulate sending the response body. The buffer size for the data pipe
  // should be larger than the body to send the whole body in one chunk.
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = body_from_net.size() * 2;
  mojo::ScopedDataPipeConsumerHandle body_consumer;
  mojo::ScopedDataPipeProducerHandle body_producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, body_producer, body_consumer));
  client->OnReceiveResponse(std::move(head), std::move(body_consumer),
                            std::nullopt);
  mojo::BlockingCopyFromString(body_from_net, body_producer);
  body_producer.reset();

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body ("abc"). At this point, data from the network reaches
  // the end.
  compare_reader_rawptr->CompletePendingRead();

  // Comparison should not finish until OnComplete() is called.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Call OnComplete() to complete the comparison. The new script should be
  // different.
  client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent,
            check_result.value().result);
  EXPECT_EQ(kScriptURL, check_result.value().url);

  // The update checker realizes that the script is different before reaching
  // the end of the script from the disk cache.
  EXPECT_FALSE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       Different_SingleRead_DifferentBody) {
  // Response body from the network.
  const std::string body_from_net = "abc";

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{"abx"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body ("abx").
  compare_reader_rawptr->CompletePendingRead();

  // Complete the comparison of the body. It should be different.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       Different_MultipleRead_NetworkIsLonger) {
  // Response body from the network.
  const std::string body_from_net = "abcdef";

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{"ab", "c", ""};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage ("ab"), and then blocked on reading the
  // body again.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage ("c"), and then blocked on reading the body
  // again.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage (""). The cache writer detects the end of
  // the body from the disk cache.
  compare_reader_rawptr->CompletePendingRead();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       Different_MultipleRead_StorageIsLonger) {
  // Response body from the network.
  const std::string body_from_net = "abc";

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{"ab", "c", "def"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage ("ab"), and then blocked on reading the
  // body again.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage ("c"). At this point, data from the network
  // reaches the end.
  compare_reader_rawptr->CompletePendingRead();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);

  // The update checker realizes that the script is different before reaching
  // the end of the script from the disk cache.
  EXPECT_FALSE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       Different_MultipleRead_DifferentBody) {
  // Response body from the network.
  const std::string body_from_net = "abc";

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{"ab", "x"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage ("ab"), and then blocked on reading the
  // body again.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage ("x"), which is different from the body
  // from the network.
  compare_reader_rawptr->CompletePendingRead();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       PendingReadWithErrorStatusShouldNotLeak) {
  // Response body from the network.
  const std::string body_from_net("abc");

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{"ab", "c"};

  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // The update checker sends a request to the loader. The testing loader keeps
  // the request.
  base::RunLoop().RunUntilIdle();
  network::TestURLLoaderFactory::PendingRequest* request =
      loader_factory->GetPendingRequest(0);
  ASSERT_TRUE(request);

  // Simulate to send the head and the body back to the checker.
  // Note that OnComplete() is not called yet.
  {
    auto head = network::CreateURLResponseHead(net::HTTP_OK);
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(kSuccessHeader));
    head->headers->GetMimeType(&head->mime_type);
    head->parsed_headers = network::mojom::ParsedHeaders::New();

    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = body_from_net.size();
    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(&options, producer, consumer));
    EXPECT_EQ(MOJO_RESULT_OK,
              producer->WriteAllData(base::as_byte_span(body_from_net)));
    // Ok to ignore `actually_written_bytes` because of `...ALL_OR_NONE`.
    request->client->OnReceiveResponse(std::move(head), std::move(consumer),
                                       std::nullopt);
  }

  // Blocked on reading the header from the storage due to the asynchronous
  // read.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Update check stops in CompareReader() due to the asynchronous read of the
  // |compare_reader|.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Return failed status code at this point. The update checker will throw the
  // internal state away.
  request->client->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_ABORTED));
  base::RunLoop().RunUntilIdle();

  // Resume the pending read. This should not crash and return kFailed.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(ServiceWorkerSingleScriptUpdateChecker::Result::kFailed,
            check_result.value().result);
}

// Tests cache validation behavior when updateViaCache is 'all'.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, UpdateViaCache_All) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load the main script. Should not validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kAll, base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_FALSE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should not validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, kScriptURL, GURL(kScope),
      false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kAll, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceWriter>(), loader_factory.get(),
      ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
          kDefault,
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_FALSE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests cache validation behavior when updateViaCache is 'none'.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, UpdateViaCache_None) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load the main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, kScriptURL, GURL(kScope),
      false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceWriter>(), loader_factory.get(),
      ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
          kDefault,
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests cache validation behavior when updateViaCache is 'imports'.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, UpdateViaCache_Imports) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kImports,
          base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should not validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, kScriptURL, GURL(kScope),
      false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceWriter>(), loader_factory.get(),
      ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
          kDefault,
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_FALSE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests attributions of the resource request for kClassic script.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, ScriptType_Classic_Main) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kImports,
          base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_EQ("script", request->headers.GetHeader("Service-Worker"));
  EXPECT_EQ(request->mode, network::mojom::RequestMode::kSameOrigin);
  EXPECT_EQ(request->credentials_mode,
            network::mojom::CredentialsMode::kSameOrigin);
  EXPECT_EQ(request->destination,
            network::mojom::RequestDestination::kServiceWorker);
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kServiceWorker));
}

// Tests attributions of the resource request for kClassic script's
// importScripts().
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       ScriptType_Classic_StaticImport) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load imported script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kImportedScriptURL, kScriptURL, GURL(kScope),
          false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kImports,
          base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_EQ(request->headers.GetHeader("Service-Worker"), std::nullopt);
  EXPECT_EQ(request->mode, network::mojom::RequestMode::kNoCors);
  EXPECT_EQ(request->credentials_mode,
            network::mojom::CredentialsMode::kInclude);
  EXPECT_EQ(request->destination, network::mojom::RequestDestination::kScript);
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kScript));
}

// Tests attributions of the resource request for kModule script.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, ScriptType_Module_Main) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kModule,
          blink::mojom::ServiceWorkerUpdateViaCache::kImports,
          base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_EQ("script", request->headers.GetHeader("Service-Worker"));
  EXPECT_EQ(request->mode, network::mojom::RequestMode::kSameOrigin);
  EXPECT_EQ(request->credentials_mode, network::mojom::CredentialsMode::kOmit);
  EXPECT_EQ(request->destination,
            network::mojom::RequestDestination::kServiceWorker);
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kServiceWorker));
}

// Tests attributions of the resource request for kModule script's static
// import.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest,
       ScriptType_Module_StaticImport) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load imported script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kImportedScriptURL, kScriptURL, GURL(kScope),
          false /* force_bypass_cache */, blink::mojom::ScriptType::kModule,
          blink::mojom::ServiceWorkerUpdateViaCache::kImports,
          base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_EQ(request->headers.GetHeader("Service-Worker"), std::nullopt);
  EXPECT_EQ(request->mode, network::mojom::RequestMode::kCors);
  EXPECT_EQ(request->credentials_mode, network::mojom::CredentialsMode::kOmit);
  EXPECT_EQ(request->destination,
            network::mojom::RequestDestination::kServiceWorker);
  EXPECT_EQ(request->resource_type,
            static_cast<int>(blink::mojom::ResourceType::kServiceWorker));
}

// Tests cache validation behavior when version's
// |force_bypass_cache_for_scripts_| is true.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, ForceBypassCache) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), true /* force_bypass_cache */,
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kAll, base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, kScriptURL, GURL(kScope),
      true /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kAll, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceWriter>(), loader_factory.get(),
      ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
          kDefault,
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests cache validation behavior when more than 24 hours passed.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, MoreThan24Hours) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kAll,
          base::Days(1) + base::Hours(1),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, kScriptURL, GURL(kScope),
      false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kAll,
      base::Days(1) + base::Hours(1),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceWriter>(), loader_factory.get(),
      ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
          kDefault,
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests MIME type header checking.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, MimeTypeError) {
  // Response body from the network.
  const std::string kBodyFromNet = "abcdef";

  // It should report error for no/bad MIME types.
  const char* kNoMimeHeader = "HTTP/1.1 200 OK\n\n";
  const char* kBadMimeHeader =
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/css\n\n";
  const std::string headers[] = {kNoMimeHeader, kBadMimeHeader};

  for (const std::string& header : headers) {
    std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
        CreateLoaderFactoryWithRespone(GURL(kScriptURL), header, kBodyFromNet,
                                       net::OK);

    auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
    auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
    auto writer = std::make_unique<MockServiceWorkerResourceWriter>();

    std::optional<CheckResult> check_result;
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
        CreateSingleScriptUpdateChecker(
            kScriptURL, kScriptURL, GURL(kScope),
            false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
            blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
            std::move(compare_reader), std::move(copy_reader),
            std::move(writer), loader_factory.get(),
            ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
                kDefault,
            &check_result);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(check_result.has_value());
    EXPECT_EQ(check_result.value().result,
              ServiceWorkerSingleScriptUpdateChecker::Result::kFailed);
    EXPECT_EQ(check_result.value().failure_info->status,
              blink::ServiceWorkerStatusCode::kErrorSecurity);
  }
}

// Tests path restriction check error for main script.
// |kOutScope| is not under the default scope ("/in-scope/") and the
// Service-Worker-Allowed header is not specified. The check should fail.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, PathRestrictionError) {
  // Response body from the network.
  const std::string kBodyFromNet = "abcdef";
  const char kMainScriptURL[] = "https://example.com/in-scope/worker.js";
  const char kOutScope[] = "https://example.com/out-scope/";
  const char kHeader[] =
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/javascript\n\n";
  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kMainScriptURL), kHeader,
                                     kBodyFromNet, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kMainScriptURL, kMainScriptURL, GURL(kOutScope),
          false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
          std::move(compare_reader), std::move(copy_reader), std::move(writer),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kFailed);
  EXPECT_EQ(check_result.value().failure_info->status,
            blink::ServiceWorkerStatusCode::kErrorSecurity);
}

// Tests path restriction check success for main script.
// |kOutScope| is not under the default scope ("/in-scope/") but the
// Service-Worker-Allowed header allows it. The check should pass.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, PathRestrictionPass) {
  // Response body from the network.
  const std::string body_from_net("abcdef");
  const char kMainScriptURL[] = "https://example.com/in-scope/worker.js";
  const char kOutScope[] = "https://example.com/out-scope/";
  const char kHeader[] =
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/javascript\n"
      "Service-Worker-Allowed: /out-scope/\n\n";

  // Stored data for |kMainScriptURL|.
  const std::vector<std::string> body_from_storage{body_from_net};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kMainScriptURL), kHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kMainScriptURL, kMainScriptURL, GURL(kOutScope),
          false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
          std::move(compare_reader), std::move(copy_reader), std::move(writer),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();

  // Unblock the body from the storage ("abcdef"), and the comparison ends.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical);
  EXPECT_EQ(check_result.value().url, kMainScriptURL);
  EXPECT_EQ(check_result.value().failure_info, nullptr);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

// Tests network error is reported.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, NetworkError) {
  // Response body from the network.
  const std::string kBodyFromNet = "abcdef";
  const char kFailHeader[] = "HTTP/1.1 404 Not Found\n\n";
  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kFailHeader,
                                     kBodyFromNet, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
          std::move(compare_reader), std::move(copy_reader), std::move(writer),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kFailed);
  EXPECT_EQ(check_result.value().failure_info->status,
            blink::ServiceWorkerStatusCode::kErrorNetwork);
}

// The main script needs to request a SSL info so that the navigation handled
// by the service worker can use the SSL info served for the main script.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, RequestSSLInfo_Classic) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load the main script. It needs a SSL info.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);
  base::RunLoop().RunUntilIdle();

  {
    ASSERT_EQ(1u, loader_factory->pending_requests()->size());
    const network::TestURLLoaderFactory::PendingRequest* pending_request =
        loader_factory->GetPendingRequest(0);
    EXPECT_EQ(kScriptURL, pending_request->request.url);
    EXPECT_EQ(network::mojom::kURLLoadOptionSendSSLInfoWithResponse,
              pending_request->options);
  }

  // Load imported script. It doesn't need SSL info.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, kScriptURL, GURL(kScope),
      false /* force_bypass_cache */, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceWriter>(), loader_factory.get(),
      ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
          kDefault,
      &check_result);
  base::RunLoop().RunUntilIdle();

  {
    ASSERT_EQ(2u, loader_factory->pending_requests()->size());
    const network::TestURLLoaderFactory::PendingRequest* pending_request =
        loader_factory->GetPendingRequest(1);
    EXPECT_EQ(kImportedScriptURL, pending_request->request.url);
    EXPECT_EQ(network::mojom::kURLLoadOptionNone, pending_request->options);
  }
}

// The module script needs to request a SSL info so that the navigation handled
// by the service worker can use the SSL info served for the module script.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, RequestSSLInfo_Module) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  std::optional<CheckResult> check_result;

  // Load the main script. It needs a SSL info.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, kScriptURL, GURL(kScope), false /* force_bypass_cache */,
          blink::mojom::ScriptType::kModule,
          blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceReader>(),
          std::make_unique<MockServiceWorkerResourceWriter>(),
          loader_factory.get(),
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
              kDefault,
          &check_result);
  base::RunLoop().RunUntilIdle();

  {
    ASSERT_EQ(1u, loader_factory->pending_requests()->size());
    const network::TestURLLoaderFactory::PendingRequest* pending_request =
        loader_factory->GetPendingRequest(0);
    EXPECT_EQ(kScriptURL, pending_request->request.url);
    EXPECT_EQ(network::mojom::kURLLoadOptionSendSSLInfoWithResponse,
              pending_request->options);
  }

  // Load imported script. It doesn't need SSL info.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, kScriptURL, GURL(kScope),
      false /* force_bypass_cache */, blink::mojom::ScriptType::kModule,
      blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceReader>(),
      std::make_unique<MockServiceWorkerResourceWriter>(), loader_factory.get(),
      ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
          kDefault,
      &check_result);
  base::RunLoop().RunUntilIdle();

  {
    ASSERT_EQ(2u, loader_factory->pending_requests()->size());
    const network::TestURLLoaderFactory::PendingRequest* pending_request =
        loader_factory->GetPendingRequest(1);
    EXPECT_EQ(kImportedScriptURL, pending_request->request.url);
    EXPECT_EQ(network::mojom::kURLLoadOptionNone, pending_request->options);
  }
}

class ServiceWorkerSingleScriptUpdateCheckerSha256ChecksumTest
    : public ServiceWorkerSingleScriptUpdateCheckerTest,
      public testing::WithParamInterface<
          ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption> {
 public:
  // Create an update checker which will always ask HTTP cache validation.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker>
  CreateSingleScriptUpdateCheckerWithoutHttpCache(
      const char* url,
      const GURL& scope,
      std::unique_ptr<MockServiceWorkerResourceReader> compare_reader,
      std::unique_ptr<MockServiceWorkerResourceReader> copy_reader,
      std::unique_ptr<MockServiceWorkerResourceWriter> writer,
      network::TestURLLoaderFactory* loader_factory,
      std::optional<CheckResult>* out_check_result) {
    return CreateSingleScriptUpdateChecker(
        url, url, scope, false /* force_bypass_cache */,
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kNone,
        base::TimeDelta() /* time_since_last_check */,
        std::move(compare_reader), std::move(copy_reader), std::move(writer),
        loader_factory, GetScriptChecksumOption(), out_check_result);
  }

  ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption
  GetScriptChecksumOption() {
    return GetParam();
  }
};

TEST_P(ServiceWorkerSingleScriptUpdateCheckerSha256ChecksumTest, Identical) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{body_from_net};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body.
  compare_reader_rawptr->CompletePendingRead();

  // Complete the comparison of the body. It should be identical.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());

  // Check if the checksum string is passed or not.
  switch (GetScriptChecksumOption()) {
    case ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
        kForceUpdate:
      EXPECT_EQ(
          // Expected hash string from SHA256("abcdef");
          "BEF57EC7F53A6D40BEB640A780A639C83BC29AC8A9816F1FC6C5C6DCD93C4721",
          check_result.value().sha256_checksum);
      break;
    case ServiceWorkerSingleScriptUpdateChecker::ScriptChecksumUpdateOption::
        kDefault:
      EXPECT_FALSE(check_result.value().sha256_checksum);
      break;
  }
}

TEST_P(ServiceWorkerSingleScriptUpdateCheckerSha256ChecksumTest, Different) {
  // Response body from the network.
  const std::string body_from_net = "abcdef";

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{"abc", ""};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResourceReader>();
  auto writer = std::make_unique<MockServiceWorkerResourceWriter>();
  MockServiceWorkerResourceReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage,
                               TotalBytes(body_from_storage));

  std::optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, GURL(kScope), std::move(compare_reader),
          std::move(copy_reader), std::move(writer), loader_factory.get(),
          &check_result);

  // Blocked on reading the header.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the header, and then blocked on reading the body.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body ("abc").
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Unblock the body from storage (""). The cache writer detects the end of
  // the body from the disk cache.
  compare_reader_rawptr->CompletePendingRead();

  // Complete the comparison of the body. It should be different.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());

  // Check if the checksum string is passed or not.
  EXPECT_FALSE(check_result.value().sha256_checksum);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerSingleScriptUpdateCheckerSha256ChecksumTest,
    testing::Values(ServiceWorkerSingleScriptUpdateChecker::
                        ScriptChecksumUpdateOption::kDefault,
                    ServiceWorkerSingleScriptUpdateChecker::
                        ScriptChecksumUpdateOption::kForceUpdate));
}  // namespace
}  // namespace content
