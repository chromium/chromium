// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_single_script_update_checker.h"

#include <vector>
#include "base/containers/queue.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_util.h"
#include "services/network/test/test_url_loader_factory.h"

namespace content {
namespace {

constexpr char kScriptURL[] = "https://example.com/script.js";
constexpr char kSuccessHeader[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/javascript\n\n";

class ServiceWorkerSingleScriptUpdateCheckerTest : public testing::Test {
 public:
  ServiceWorkerSingleScriptUpdateCheckerTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ServiceWorkerSingleScriptUpdateCheckerTest() override = default;

  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    base::RunLoop run_loop;
    storage()->LazyInitializeForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  size_t TotalBytes(const std::vector<std::string>& data_chunks) {
    size_t bytes = 0;
    for (const auto& data : data_chunks)
      bytes += data.size();
    return bytes;
  }

  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker>
  CreateSingleScriptUpdateChecker(
      const char* url,
      std::unique_ptr<ServiceWorkerResponseReader> compare_reader,
      std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
      std::unique_ptr<ServiceWorkerResponseWriter> writer,
      network::TestURLLoaderFactory* loader_factory,
      base::Optional<bool>* out_script_changed) {
    helper_->SetNetworkFactory(loader_factory);
    return std::make_unique<ServiceWorkerSingleScriptUpdateChecker>(
        GURL(url), true /* is_main_script */,
        helper_->url_loader_factory_getter()->GetNetworkFactory(),
        std::move(compare_reader), std::move(copy_reader), std::move(writer),
        base::BindOnce(
            [](base::Optional<bool>* out_script_changed, bool script_changed) {
              *out_script_changed = script_changed;
            },
            out_script_changed));
  }

  std::unique_ptr<network::TestURLLoaderFactory> CreateLoaderFactoryWithRespone(
      const GURL& url,
      std::string header,
      std::string body,
      net::Error error) {
    auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
    network::ResourceResponseHead head;
    head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(header.c_str(), header.size()));
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = body.size();
    loader_factory->AddResponse(url, head, body, status);
    return loader_factory;
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSingleScriptUpdateCheckerTest);
};

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Identical_SingleSyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{body_from_net};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               false /* async */);

  base::Optional<bool> script_changed;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(kScriptURL, std::move(compare_reader),
                                      std::move(copy_reader), std::move(writer),
                                      loader_factory.get(), &script_changed);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(script_changed.has_value());
  EXPECT_FALSE(script_changed.value());
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Different_SingleSyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{"abxx"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               false /* async */);

  base::Optional<bool> script_changed;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(kScriptURL, std::move(compare_reader),
                                      std::move(copy_reader), std::move(writer),
                                      loader_factory.get(), &script_changed);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(script_changed.has_value());
  EXPECT_TRUE(script_changed.value());
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Different_MultipleSyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL| (the data for compare reader).
  // The comparison should stop in the second block of data.
  const std::vector<std::string> body_from_storage{"ab", "cx"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               false /* async */);

  base::Optional<bool> script_changed;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(kScriptURL, std::move(compare_reader),
                                      std::move(copy_reader), std::move(writer),
                                      loader_factory.get(), &script_changed);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(script_changed.has_value());
  EXPECT_TRUE(script_changed.value());
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, NetworkDataLong_SyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{"ab", "cd", ""};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               false /* async */);

  base::Optional<bool> script_changed;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(kScriptURL, std::move(compare_reader),
                                      std::move(copy_reader), std::move(writer),
                                      loader_factory.get(), &script_changed);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(script_changed.has_value());
  EXPECT_TRUE(script_changed.value());
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, NetworkDataShort_SyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_in_storage{"ab", "cd", "ef", "gh"};

  // Stored data that will actually be read from the compare reader.
  // The last 2 bytes of |body_in_storage| won't be read.
  const std::vector<std::string> body_read_from_storage{"ab", "cd", "ef"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_read_from_storage,
                               TotalBytes(body_in_storage), false /* async */);

  base::Optional<bool> script_changed;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(kScriptURL, std::move(compare_reader),
                                      std::move(copy_reader), std::move(writer),
                                      loader_factory.get(), &script_changed);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(script_changed.has_value());
  EXPECT_TRUE(script_changed.value());
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Identical_SingleAsyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{body_from_net};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               true /* async */);

  base::Optional<bool> script_changed;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(kScriptURL, std::move(compare_reader),
                                      std::move(copy_reader), std::move(writer),
                                      loader_factory.get(), &script_changed);

  // Update check stops in WriteHeader() due to the asynchronous read of the
  // |compare_reader|.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(script_changed.has_value());

  // Continue the update check and trigger OnWriteHeadersComplete(). The resumed
  // update check stops again at CompareData().
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(script_changed.has_value());

  // Continue the update check and trigger OnCompareDataComplete(). This will
  // finish the entire update check.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(script_changed.has_value());
  EXPECT_FALSE(script_changed.value());
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

}  // namespace
}  // namespace content
