// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

// Data encoded in kBrotliHelloData.
const char kHelloData[] = "hello, world!\n";
// kHelloData encoded with brotli.
const char kBrotliHelloData[] =
    "\033\015\0\0\244\024\102\152\020\111\152\072\235\126\034";

// This is a header that signals the end of the data.
const char kGzipData[] = "\x1f\x08b\x08\0\0\0\0\0\0\3\3\0\0\0\0\0\0\0\0";
const char kGzipDataWithName[] =
    "\x1f\x08b\x08\x08\0\0\0\0\0\0name\0\3\0\0\0\0\0\0\0\0";
// kHelloData encoded with gzip.
const char kGzipHelloData[] =
    "\x1f\x8b\x08\x08\x46\x7d\x4e\x56\x00\x03\x67\x7a\x69\x70\x2e\x74\x78\x74"
    "\x00\xcb\x48\xcd\xc9\xc9\xe7\x02\x00\x20\x30\x3a\x36\x06\x00\x00\x00";

void GZipServer(const HttpRequestInfo* request,
                std::string* response_status,
                std::string* response_headers,
                std::string* response_data) {
  response_data->assign(kGzipData, sizeof(kGzipData));
}

void GZipHelloServer(const HttpRequestInfo* request,
                     std::string* response_status,
                     std::string* response_headers,
                     std::string* response_data) {
  response_data->assign(kGzipHelloData, sizeof(kGzipHelloData) - 1);
}

void BigGZipServer(const HttpRequestInfo* request,
                   std::string* response_status,
                   std::string* response_headers,
                   std::string* response_data) {
  response_data->assign(kGzipDataWithName, sizeof(kGzipDataWithName));
  response_data->insert(10, 64 * 1024, 'a');
}

void BrotliHelloServer(const HttpRequestInfo* request,
                       std::string* response_status,
                       std::string* response_headers,
                       std::string* response_data) {
  response_data->assign(kBrotliHelloData, sizeof(kBrotliHelloData) - 1);
}

void MakeMockReferrerPolicyTransaction(const char* referer_header,
                                       const char* response_headers,
                                       MockTransaction* transaction) {
  transaction->method = "GET";
  transaction->request_time = base::Time();
  transaction->request_headers = referer_header;
  transaction->load_flags = LOAD_NORMAL;
  transaction->status = "HTTP/1.1 302 Found";
  transaction->response_headers = response_headers;
  transaction->response_time = base::Time();
  transaction->data = "hello";
  transaction->dns_aliases = {};
  transaction->test_mode = TEST_MODE_NORMAL;
  transaction->handler = MockTransactionHandler();
  transaction->read_handler = MockTransactionReadHandler();
  if (GURL(transaction->url).SchemeIsCryptographic()) {
    transaction->cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  } else {
    transaction->cert = nullptr;
  }
  transaction->cert_status = 0;
  transaction->ssl_connection_status = 0;
  transaction->start_return_code = OK;
}

const MockTransaction kNoFilterTransaction = {
    "http://www.google.com/gzyp",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Length: 30\n",  // Intentionally wrong.
    base::Time(),
    "hello",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    OK,
    OK,
};

const MockTransaction kNoFilterTransactionWithInvalidLength = {
    "http://www.google.com/gzyp",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Length: +30\n",  // Invalid
    base::Time(),
    "hello",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    OK,
    OK,
};

const MockTransaction kGZipTransaction = {
    "http://www.google.com/gzyp",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Encoding: gzip\n"
    "Content-Length: 30\n",  // Intentionally wrong.
    base::Time(),
    "",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    base::BindRepeating(&GZipServer),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kGzipSlowTransaction = {
    "http://www.google.com/gzyp",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Encoding: gzip\n",
    base::Time(),
    "",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_SLOW_READ,
    base::BindRepeating(&GZipHelloServer),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kRedirectTransaction = {
    "http://www.google.com/redirect",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 302 Found",
    "Cache-Control: max-age=10000\n"
    "Location: http://www.google.com/destination\n"
    "Content-Length: 5\n",
    base::Time(),
    "hello",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kEmptyBodyGzipTransaction = {
    "http://www.google.com/empty_body",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Content-Encoding: gzip\n",
    base::Time(),
    "",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kInvalidContentGZipTransaction = {
    "http://www.google.com/gzyp",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Content-Encoding: gzip\n"
    "Content-Length: 21\n",
    base::Time(),
    "not a valid gzip body",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_NORMAL,
    MockTransactionHandler(),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

const MockTransaction kBrotliSlowTransaction = {
    "http://www.google.com/brotli",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    DefaultTransportInfo(),
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Encoding: br\n"
    "Content-Length: 230\n",  // Intentionally wrong.
    base::Time(),
    "",
    {},
    std::nullopt,
    std::nullopt,
    TEST_MODE_SLOW_READ,
    base::BindRepeating(&BrotliHelloServer),
    MockTransactionReadHandler(),
    nullptr,
    0,
    0,
    OK,
    OK,
};

}  // namespace

using URLRequestJobTest = TestWithTaskEnvironment;

TEST_F(URLRequestJobTest, TransactionNoFilter) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kNoFilterTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ("hello", d.data_received());
  EXPECT_TRUE(network_layer->done_reading_called());
  // When there's no filter and a Content-Length, expected content size should
  // be available.
  EXPECT_EQ(30, req->GetExpectedContentSize());
}

TEST_F(URLRequestJobTest, TransactionNoFilterWithInvalidLength) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kNoFilterTransactionWithInvalidLength);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ("hello", d.data_received());
  EXPECT_TRUE(network_layer->done_reading_called());
  // Invalid Content-Lengths that start with a + should not be reported.
  EXPECT_EQ(-1, req->GetExpectedContentSize());
}

TEST_F(URLRequestJobTest, TransactionNotifiedWhenDone) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kGZipTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ("", d.data_received());
  EXPECT_TRUE(network_layer->done_reading_called());
  // When there's a filter and a Content-Length, expected content size should
  // not be available.
  EXPECT_EQ(-1, req->GetExpectedContentSize());
}

TEST_F(URLRequestJobTest, SyncTransactionNotifiedWhenDone) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kGZipTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(kGZipTransaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  transaction.test_mode = TEST_MODE_SYNC_ALL;

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ("", d.data_received());
  EXPECT_TRUE(network_layer->done_reading_called());
  // When there's a filter and a Content-Length, expected content size should
  // not be available.
  EXPECT_EQ(-1, req->GetExpectedContentSize());
}

// Tests processing a large gzip header one byte at a time.
TEST_F(URLRequestJobTest, SyncSlowTransaction) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kGZipTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  transaction.test_mode = TEST_MODE_SYNC_ALL | TEST_MODE_SLOW_READ;
  transaction.handler = base::BindRepeating(&BigGZipServer);

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_TRUE(d.response_completed());
  EXPECT_EQ(OK, d.request_status());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ("", d.data_received());
  EXPECT_TRUE(network_layer->done_reading_called());
  EXPECT_EQ(-1, req->GetExpectedContentSize());
}

TEST_F(URLRequestJobTest, RedirectTransactionNotifiedWhenDone) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kRedirectTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_TRUE(network_layer->done_reading_called());
}

TEST_F(URLRequestJobTest, RedirectTransactionWithReferrerPolicyHeader) {
  struct TestCase {
    const char* original_url;
    const char* original_referrer;
    const char* response_headers;
    ReferrerPolicy original_referrer_policy;
    ReferrerPolicy expected_final_referrer_policy;
    const char* expected_final_referrer;
  };

  // Note: There are more thorough test cases in RedirectInfoTest.
  const TestCase kTests[] = {
      // If a redirect serves 'Referrer-Policy: no-referrer', then the referrer
      // should be cleared.
      {"http://foo.test/one" /* original url */,
       "http://foo.test/one" /* original referrer */,
       "Location: http://foo.test/test\n"
       "Referrer-Policy: no-referrer\n",
       // original policy
       ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       ReferrerPolicy::NO_REFERRER /* expected final policy */,
       "" /* expected final referrer */},

      // A redirect response without Referrer-Policy header should not affect
      // the policy and the referrer.
      {"http://foo.test/one" /* original url */,
       "http://foo.test/one" /* original referrer */,
       "Location: http://foo.test/test\n",
       // original policy
       ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       // expected final policy
       ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
       "http://foo.test/one" /* expected final referrer */},
  };

  for (const auto& test : kTests) {
    ScopedMockTransaction transaction(test.original_url);
    std::string request_headers =
        "Referer: " + std::string(test.original_referrer) + "\n";
    MakeMockReferrerPolicyTransaction(request_headers.c_str(),
                                      test.response_headers, &transaction);

    auto context_builder = CreateTestURLRequestContextBuilder();
    auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
        std::make_unique<MockNetworkLayer>());
    context_builder->DisableHttpCache();
    auto context = context_builder->Build();

    TestDelegate d;
    std::unique_ptr<URLRequest> req(
        context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                               TRAFFIC_ANNOTATION_FOR_TESTS));

    req->set_referrer_policy(test.original_referrer_policy);
    req->SetReferrer(test.original_referrer);

    req->set_method("GET");
    req->Start();

    d.RunUntilComplete();

    EXPECT_TRUE(network_layer->done_reading_called());

    // Test that the referrer policy and referrer were set correctly
    // according to the header received during the redirect.
    EXPECT_EQ(test.expected_final_referrer_policy, req->referrer_policy());
    EXPECT_EQ(test.expected_final_referrer, req->referrer());
  }
}

TEST_F(URLRequestJobTest, TransactionNotCachedWhenNetworkDelegateRedirects) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  auto network_delegate = std::make_unique<TestNetworkDelegate>();
  network_delegate->set_redirect_on_headers_received_url(GURL("http://foo"));
  context_builder->DisableHttpCache();
  context_builder->set_network_delegate(std::move(network_delegate));
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kGZipTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_TRUE(network_layer->stop_caching_called());
}

// Makes sure that ReadRawDataComplete correctly updates request status before
// calling ReadFilteredData.
// Regression test for crbug.com/553300.
TEST_F(URLRequestJobTest, EmptyBodySkipFilter) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kEmptyBodyGzipTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_TRUE(d.data_received().empty());
  EXPECT_TRUE(network_layer->done_reading_called());
}

// Regression test for crbug.com/575213.
TEST_F(URLRequestJobTest, InvalidContentGZipTransaction) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kInvalidContentGZipTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  // Request failed indicates the request failed before headers were received,
  // so should be false.
  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, d.request_status());
  EXPECT_TRUE(d.data_received().empty());
  EXPECT_FALSE(network_layer->done_reading_called());
}

// Regression test for crbug.com/553300.
TEST_F(URLRequestJobTest, SlowFilterRead) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kGzipSlowTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ("hello\n", d.data_received());
  EXPECT_TRUE(network_layer->done_reading_called());
}

TEST_F(URLRequestJobTest, SlowBrotliRead) {
  auto context_builder = CreateTestURLRequestContextBuilder();
  auto* network_layer = context_builder->SetHttpTransactionFactoryForTesting(
      std::make_unique<MockNetworkLayer>());
  context_builder->DisableHttpCache();
  auto context = context_builder->Build();

  ScopedMockTransaction transaction(kBrotliSlowTransaction);
  TestDelegate d;
  std::unique_ptr<URLRequest> req(
      context->CreateRequest(GURL(transaction.url), DEFAULT_PRIORITY, &d,
                             TRAFFIC_ANNOTATION_FOR_TESTS));

  req->set_method("GET");
  req->Start();

  d.RunUntilComplete();

  EXPECT_FALSE(d.request_failed());
  EXPECT_EQ(200, req->GetResponseCode());
  EXPECT_EQ(kHelloData, d.data_received());
  EXPECT_TRUE(network_layer->done_reading_called());
  // When there's a filter and a Content-Length, expected content size should
  // not be available.
  EXPECT_EQ(-1, req->GetExpectedContentSize());
}

TEST(URLRequestJobComputeReferrer, SetsSameOriginForMetricsOnSameOrigin) {
  bool same_origin = false;
  URLRequestJob::ComputeReferrerForPolicy(
      ReferrerPolicy(),
      /*original_referrer=*/GURL("http://google.com"),
      /*destination=*/GURL("http://google.com"), &same_origin);
  EXPECT_TRUE(same_origin);
}

TEST(URLRequestJobComputeReferrer, SetsSameOriginForMetricsOnCrossOrigin) {
  bool same_origin = true;
  URLRequestJob::ComputeReferrerForPolicy(
      ReferrerPolicy(),
      /*original_referrer=*/GURL("http://google.com"),
      /*destination=*/GURL("http://boggle.com"), &same_origin);
  EXPECT_FALSE(same_origin);
}

TEST(URLRequestJobComputeReferrer, AcceptsNullptrInput) {
  // Shouldn't segfault.
  URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy(), GURL(), GURL(),
                                          nullptr);
}

TEST(URLRequestJobComputeReferrer, FilesystemDestination) {
  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(
                ReferrerPolicy::NEVER_CLEAR, GURL("https://referrer.example"),
                GURL("filesystem:https://destination.example"), nullptr),
            GURL("https://referrer.example"));
}

TEST(URLRequestJobComputeReferrer, TruncatesLongReferrer) {
  std::string original_spec = "https://referrer.example/";
  original_spec.resize(4097, 'a');
  const GURL kOriginalReferrer(original_spec);

  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy::NEVER_CLEAR,
                                                    kOriginalReferrer,
                                                    GURL("https://google.com")),
            GURL("https://referrer.example/"));
}

TEST(URLRequestJobComputeReferrer, DoesntTruncateShortReferrer) {
  std::string original_spec = "https://referrer.example/";
  original_spec.resize(4096, 'a');
  const GURL kOriginalReferrer(original_spec);

  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy::NEVER_CLEAR,
                                                    kOriginalReferrer,
                                                    GURL("https://google.com")),
            kOriginalReferrer);
}

TEST(URLRequestJobComputeReferrer, DoesntTruncateEvenShorterReferrer) {
  std::string original_spec = "https://referrer.example/";
  original_spec.resize(4095, 'a');
  const GURL kOriginalReferrer(original_spec);

  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy::NEVER_CLEAR,
                                                    kOriginalReferrer,
                                                    GURL("https://google.com")),
            kOriginalReferrer);
}

TEST(URLRequestJobComputeReferrer, DoesntTruncateReferrerWithLongRef) {
  // Because the "is the length greater than 4096?" check comes *after*
  // stripping the ref in the Referrer Policy spec, a URL that is short except
  // for having a very long ref should not be stripped to an origin by the "if
  // the length is too long, strip to the origin" check.
  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(
                ReferrerPolicy::NEVER_CLEAR,
                GURL(std::string("https://referrer.example/path#") +
                     std::string(5000, 'a')),
                GURL("https://google.com")),
            GURL("https://referrer.example/path"));
}

TEST(URLRequestJobComputeReferrer, InvalidSchemeReferrer) {
  const GURL kOriginalReferrer("about:blank");
  ASSERT_FALSE(url::IsReferrerScheme(
      kOriginalReferrer.spec().data(),
      kOriginalReferrer.parsed_for_possibly_invalid_spec().scheme));

  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy::NEVER_CLEAR,
                                                    kOriginalReferrer,
                                                    GURL("https://google.com")),
            GURL());

  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy::ORIGIN,
                                                    kOriginalReferrer,
                                                    GURL("https://google.com")),
            GURL());
}

TEST(URLRequestJobComputeReferrer, CapReferrerOnCrossOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kCapReferrerToOriginOnCrossOrigin);

  const GURL kOriginalReferrer("https://boggle.com/path");

  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy::NEVER_CLEAR,
                                                    kOriginalReferrer,
                                                    GURL("https://google.com")),
            GURL("https://boggle.com/"));
}

TEST(URLRequestJobComputeReferrer,
     CapReferrerOnCrossOriginRespectsStricterPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kCapReferrerToOriginOnCrossOrigin);

  const GURL kOriginalReferrer("https://boggle.com/path");

  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy::NO_REFERRER,
                                                    kOriginalReferrer,
                                                    GURL("https://google.com")),
            GURL());
}

TEST(URLRequestJobComputeReferrer,
     CapReferrerOnCrossOriginDoesntCapOnSameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kCapReferrerToOriginOnCrossOrigin);

  const GURL kOriginalReferrer("https://boggle.com/path");

  EXPECT_EQ(URLRequestJob::ComputeReferrerForPolicy(ReferrerPolicy::NEVER_CLEAR,
                                                    kOriginalReferrer,
                                                    GURL("https://boggle.com")),
            kOriginalReferrer);
}

}  // namespace net
