// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/shared_dictionary/shared_dictionary_network_transaction.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "crypto/hash.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_test_util.h"
#include "net/log/net_log_with_source.h"
#include "net/shared_dictionary/shared_dictionary.h"
#include "net/shared_dictionary/shared_dictionary_constants.h"
#include "net/shared_dictionary/shared_dictionary_transaction_outcome.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const std::string kTestDictionaryData = "HelloHallo你好こんにちは";
// The hex of sha256 of `kTestDictionaryData`.
const std::string kTestDictionarySha256 =
    "c19728aed36503cfc81a0f5359e6f472e121f77bf20a2faac7994191293c0623";
// The Structured Field sf-binary hash of sha256 of `kTestDictionaryData`.
const std::string kTestDictionarySha256Base64 =
    ":wZcortNlA8/IGg9TWeb0cuEh93vyCi+qx5lBkSk8BiM=:";
const std::string kTestData =
    "HelloこんにちはHallo你好HelloこんにちはHallo你好";
// The brotli encoded data of `kTestData` using `kTestDictionaryData` as a
// dictionary.
// kBrotliEncodedData is generated using the following commands:
// $ echo -n "HelloHallo你好こんにちは" > /tmp/dict
// $ echo -n "HelloこんにちはHallo你好HelloこんにちはHallo你好" > /tmp/data
// $ echo -en '\xffDCB' > /tmp/out.dcb
// $ openssl dgst -sha256 -binary /tmp/dict >> /tmp/out.dcb
// $ brotli --stdout -D /tmp/dict /tmp/data >> /tmp/out.dcb
// $ xxd -i /tmp/out.dcb
const uint8_t kBrotliEncodedData[] = {
    0xff, 0x44, 0x43, 0x42, 0xc1, 0x97, 0x28, 0xae, 0xd3, 0x65, 0x03, 0xcf,
    0xc8, 0x1a, 0x0f, 0x53, 0x59, 0xe6, 0xf4, 0x72, 0xe1, 0x21, 0xf7, 0x7b,
    0xf2, 0x0a, 0x2f, 0xaa, 0xc7, 0x99, 0x41, 0x91, 0x29, 0x3c, 0x06, 0x23,
    0xa1, 0xe8, 0x01, 0x00, 0x22, 0x8d, 0x54, 0xc6, 0xf6, 0x26, 0x81, 0x69,
    0x46, 0x9d, 0xb2, 0x60, 0x0e, 0x6b, 0xf5, 0x07, 0x02};
const std::string kBrotliEncodedDataString =
    std::string(reinterpret_cast<const char*>(kBrotliEncodedData),
                sizeof(kBrotliEncodedData));

// The zstd encoded data of `kTestData` using `kTestDictionaryData` as a
// dictionary.
// kZstdEncodedData is generated using the following commands:
// $ echo -n "HelloHallo你好こんにちは" > /tmp/dict
// $ echo -n "HelloこんにちはHallo你好HelloこんにちはHallo你好" > /tmp/data
// $ echo -en '\x5e\x2a\x4d\x18\x20\x00\x00\x00' > /tmp/out.dcz
// $ openssl dgst -sha256 -binary /tmp/dict >> /tmp/out.dcz
// $ zstd -D /tmp/dict -f -o /tmp/tmp.zstd /tmp/data
// $ cat /tmp/tmp.zstd >> /tmp/out.dcz
// $ xxd -i /tmp/out.dcz
const uint8_t kZstdEncodedData[] = {
    0x5e, 0x2a, 0x4d, 0x18, 0x20, 0x00, 0x00, 0x00, 0xc1, 0x97, 0x28, 0xae,
    0xd3, 0x65, 0x03, 0xcf, 0xc8, 0x1a, 0x0f, 0x53, 0x59, 0xe6, 0xf4, 0x72,
    0xe1, 0x21, 0xf7, 0x7b, 0xf2, 0x0a, 0x2f, 0xaa, 0xc7, 0x99, 0x41, 0x91,
    0x29, 0x3c, 0x06, 0x23, 0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x3e, 0x85, 0x00,
    0x00, 0x28, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x03, 0x00, 0x42, 0x35, 0x88,
    0x6a, 0x03, 0x87, 0x4c, 0x2d, 0xcd, 0x1e, 0xde, 0x25};
const std::string kZstdEncodedDataString =
    std::string(reinterpret_cast<const char*>(kZstdEncodedData),
                sizeof(kZstdEncodedData));

const size_t kDefaultBufferSize = 1023;

class DummySyncDictionary : public SharedDictionary {
 public:
  explicit DummySyncDictionary(const std::string& data_string,
                               const std::string& id = "")
      : data_(base::MakeRefCounted<StringIOBuffer>(data_string)),
        id_(id) {
    hash_ = crypto::hash::Sha256(data_->span());
  }

  // SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override { return OK; }
  scoped_refptr<IOBuffer> data() const override { return data_; }
  size_t size() const override { return data_->size(); }
  const SHA256HashValue& hash() const override { return hash_; }
  const std::string& id() const override { return id_; }

 protected:
  ~DummySyncDictionary() override = default;

 private:
  const scoped_refptr<StringIOBuffer> data_;
  const std::string id_;
  SHA256HashValue hash_;
};

class DummyAsyncDictionary : public DummySyncDictionary {
 public:
  explicit DummyAsyncDictionary(const std::string& data_string)
      : DummySyncDictionary(data_string) {}

  // SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override {
    read_all_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }
  base::OnceCallback<void(int)> TakeReadAllCallback() {
    return std::move(read_all_callback_);
  }

 private:
  ~DummyAsyncDictionary() override = default;

  base::OnceCallback<void(int)> read_all_callback_;
};

TransportInfo TestSpdyTransportInfo() {
  return TransportInfo(
      TransportType::kDirect, IPEndPoint(IPAddress::IPv4Localhost(), 80),
      /*accept_ch_frame_arg=*/"",
      /*cert_is_issued_by_known_root=*/false, NextProto::kProtoHTTP2);
}

static void BrotliTestTransactionHandler(const HttpRequestInfo* request,
                                         std::string* response_status,
                                         std::string* response_headers,
                                         std::string* response_data) {
  EXPECT_THAT(request->extra_headers.GetHeader(
                  shared_dictionary::kAvailableDictionaryHeaderName),
              testing::Optional(kTestDictionarySha256Base64));
  *response_data = kBrotliEncodedDataString;
}

static void ZstdTestTransactionHandler(const HttpRequestInfo* request,
                                       std::string* response_status,
                                       std::string* response_headers,
                                       std::string* response_data) {
  EXPECT_THAT(request->extra_headers.GetHeader(
                  shared_dictionary::kAvailableDictionaryHeaderName),
              testing::Optional(kTestDictionarySha256Base64));
  *response_data = kZstdEncodedDataString;
}

static const auto kTestTransactionHandlerWithoutAvailableDictionary =
    base::BindRepeating([](const HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data) {
      EXPECT_FALSE(request->extra_headers.HasHeader(
          shared_dictionary::kAvailableDictionaryHeaderName));
      *response_data = kTestData;
    });

constexpr char kTestUrl[] = "https://test.example/test";

const MockTransaction kBrotliDictionaryTestTransaction = {
    .url = kTestUrl,
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "sec-fetch-dest: document\r\n",
    .load_flags = LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = TestSpdyTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers = "content-encoding: dcb\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = std::nullopt,
    .browser_run_id = std::nullopt,
    .test_mode = TEST_MODE_NORMAL,
    .handler = base::BindRepeating(&BrotliTestTransactionHandler),
    .read_handler = MockTransactionReadHandler(),
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = OK,
    .read_return_code = OK,
};

const MockTransaction kZstdDictionaryTestTransaction = {
    .url = kTestUrl,
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "sec-fetch-dest: document\r\n",
    .load_flags = LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = TestSpdyTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers = "content-encoding: dcz\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = std::nullopt,
    .browser_run_id = std::nullopt,
    .test_mode = TEST_MODE_NORMAL,
    .handler = base::BindRepeating(&ZstdTestTransactionHandler),
    .read_handler = MockTransactionReadHandler(),
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = OK,
    .read_return_code = OK,
};

class SharedDictionaryNetworkTransactionTest : public ::testing::Test {
 public:
  SharedDictionaryNetworkTransactionTest()
      : scoped_mock_transaction_(kBrotliDictionaryTestTransaction),
        network_layer_(std::make_unique<MockNetworkLayer>()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            features::kCompressionDictionaryTransportRequireKnownRootCert});
  }
  ~SharedDictionaryNetworkTransactionTest() override = default;

  SharedDictionaryNetworkTransactionTest(
      const SharedDictionaryNetworkTransactionTest&) = delete;
  SharedDictionaryNetworkTransactionTest& operator=(
      const SharedDictionaryNetworkTransactionTest&) = delete;

 protected:
  std::unique_ptr<HttpTransaction> CreateNetworkTransaction() {
    return network_layer_->CreateTransaction(DEFAULT_PRIORITY);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  MockNetworkLayer& network_layer() { return *network_layer_.get(); }

  std::optional<ScopedMockTransaction> scoped_mock_transaction_;

 private:
  std::unique_ptr<MockNetworkLayer> network_layer_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SharedDictionaryNetworkTransactionTest, SyncDictionary) {
  base::HistogramTester histogram_tester;
  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionary.Transaction.Outcome",
      SharedDictionaryTransactionOutcome::kDictionaryUsedBrotli, 1);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, NotAllowedToUseDictionary) {
  base::HistogramTester histogram_tester;
  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });

  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return false; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
  histogram_tester.ExpectTotalCount("Net.SharedDictionary.Transaction.Outcome",
                                    0);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, DictionaryId) {
  // Change MockTransaction to check the dictionary-id header
  scoped_mock_transaction_->handler = base::BindRepeating(
      [](const HttpRequestInfo* request, std::string* response_status,
         std::string* response_headers, std::string* response_data) {
        EXPECT_THAT(request->extra_headers.GetHeader("dictionary-id"),
                    testing::Optional(std::string("\"test-id\"")));
        *response_data = kBrotliEncodedDataString;
      });

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData,
                                                         "test-id");
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       DictionaryIdWithBackSlashAndDquote) {
  // Change MockTransaction to check the dictionary-id header
  scoped_mock_transaction_->handler = base::BindRepeating(
      [](const HttpRequestInfo* request, std::string* response_status,
         std::string* response_headers, std::string* response_data) {
        EXPECT_THAT(
            request->extra_headers.GetHeader("dictionary-id"),
            testing::Optional(std::string("\"test\\\\dictionary\\\"id\"")));
        *response_data = kBrotliEncodedDataString;
      });

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(
            kTestDictionaryData, "test\\dictionary\"id");
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, EmptyDictionaryId) {
  // Change MockTransaction to check the dictionary-id header
  scoped_mock_transaction_->handler = base::BindRepeating(
      [](const HttpRequestInfo* request, std::string* response_status,
         std::string* response_headers, std::string* response_data) {
        EXPECT_FALSE(request->extra_headers.HasHeader("dictionary-id"));
        *response_data = kBrotliEncodedDataString;
      });

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData,
                                                         "");
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kCompressionDictionaryTransportRequireKnownRootCert);
  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;
  scoped_mock_transaction_->transport_info.cert_is_issued_by_known_root = false;

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckSuccess) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kCompressionDictionaryTransportRequireKnownRootCert);
  // The BrotliTestTransactionHandler `scoped_mock_transaction_->handler` will
  // check that the there is a correct available-dictionary request header.
  scoped_mock_transaction_->transport_info.cert_is_issued_by_known_root = true;

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckSuccessForLocalhost) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kCompressionDictionaryTransportRequireKnownRootCert);
  // The BrotliTestTransactionHandler `new_mock_transaction.handler` will check
  // that the there is a correct available-dictionary request header.
  ScopedMockTransaction scoped_mock_transaction(
      kBrotliDictionaryTestTransaction, "http:///localhost:1234/test");
  scoped_mock_transaction.transport_info.cert_is_issued_by_known_root = false;

  MockHttpRequest request(scoped_mock_transaction);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, NoMatchingDictionary) {
  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return nullptr;
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, OpaqueFrameOrigin) {
  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        // dictionary_getter must be called with a nullopt isolation_key.
        CHECK(!isolation_key);
        return nullptr;
      });
  request.frame_origin = url::Origin();
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, WithoutValidLoadFlag) {
  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;

  MockHttpRequest request(*scoped_mock_transaction_);
  bool getter_called = false;
  request.dictionary_getter = base::BindRepeating(
      [](bool* getter_called,
         const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        *getter_called = true;
        return nullptr;
      },
      base::Unretained(&getter_called));
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);

  CHECK_EQ(LOAD_CAN_USE_SHARED_DICTIONARY, request.load_flags);
  // Change load_flags not to trigger the shared dictionary logic.
  request.load_flags = LOAD_NORMAL;

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));

  // SharedDictionaryGetter must not be called when
  // LOAD_CAN_USE_SHARED_DICTIONARY is not set.
  EXPECT_FALSE(getter_called);
}

TEST_F(SharedDictionaryNetworkTransactionTest, NoSbrContentEncoding) {
  base::HistogramTester histogram_tester;
  // Change MockTransaction to remove `content-encoding: dcb`.
  scoped_mock_transaction_->response_headers = "";

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionary.Transaction.Outcome",
      SharedDictionaryTransactionOutcome::kDictionaryNotUsed, 1);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is no "content-encoding: dcb" header,
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kBrotliEncodedDataString.size());
  EXPECT_EQ(kBrotliEncodedDataString, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, WrongContentDictionaryHeader) {
  scoped_mock_transaction_->handler = base::BindRepeating(
      [](const HttpRequestInfo* request, std::string* response_status,
         std::string* response_headers, std::string* response_data) {
        std::string data = kBrotliEncodedDataString;
        // Change the first byte of the compressed data to trigger
        // UNEXPECTED_CONTENT_DICTIONARY_HEADER error.
        ++data[0];
        *response_data = data;
      });

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(start_callback.GetResult(transaction.Start(
                  &request, start_callback.callback(), NetLogWithSource())),
              test::IsError(OK));
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(read_callback.GetResult(transaction.Read(
                  buf.get(), buf->size(), read_callback.callback())),
              test::IsError(ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER));
}
TEST_F(SharedDictionaryNetworkTransactionTest, MultipleContentEncodingWithSbr) {
  // Change MockTransaction to set `content-encoding: dcb, deflate`.
  scoped_mock_transaction_->response_headers =
      "content-encoding: dcb, deflate\n";

  MockHttpRequest request(*scoped_mock_transaction_);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is Content-Encoding header which value is other than "dcb",
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kBrotliEncodedDataString.size());
  EXPECT_EQ(kBrotliEncodedDataString, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessBeforeStartReading) {
  scoped_refptr<DummyAsyncDictionary> dictionary =
      base::MakeRefCounted<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();

  MockHttpRequest request(kBrotliDictionaryTestTransaction);
  request.dictionary_getter = base::BindRepeating(
      [](scoped_refptr<DummyAsyncDictionary>* dictionary,
         const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        CHECK(*dictionary);
        return std::move(*dictionary);
      },
      base::Unretained(&dictionary));
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);
  std::move(dictionary_read_all_callback).Run(OK);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessAfterStartReading) {
  scoped_refptr<DummyAsyncDictionary> dictionary =
      base::MakeRefCounted<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();

  MockHttpRequest request(kBrotliDictionaryTestTransaction);
  request.dictionary_getter = base::BindRepeating(
      [](scoped_refptr<DummyAsyncDictionary>* dictionary,
         const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        CHECK(*dictionary);
        return std::move(*dictionary);
      },
      base::Unretained(&dictionary));
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  std::move(dictionary_read_all_callback).Run(OK);

  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessAfterTransactionDestroy) {
  scoped_refptr<DummyAsyncDictionary> dictionary =
      base::MakeRefCounted<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();

  MockHttpRequest request(kBrotliDictionaryTestTransaction);
  request.dictionary_getter = base::BindRepeating(
      [](scoped_refptr<DummyAsyncDictionary>* dictionary,
         const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        CHECK(*dictionary);
        return std::move(*dictionary);
      },
      base::Unretained(&dictionary));
  std::unique_ptr<SharedDictionaryNetworkTransaction> transaction =
      std::make_unique<SharedDictionaryNetworkTransaction>(
          CreateNetworkTransaction(), /*enable_shared_zstd=*/false);
  transaction->SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction->Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  transaction.reset();

  std::move(dictionary_read_all_callback).Run(OK);

  EXPECT_FALSE(read_callback.have_result());
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionaryFailureBeforeStartReading) {
  scoped_refptr<DummyAsyncDictionary> dictionary =
      base::MakeRefCounted<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();

  MockHttpRequest request(kBrotliDictionaryTestTransaction);
  request.dictionary_getter = base::BindRepeating(
      [](scoped_refptr<DummyAsyncDictionary>* dictionary,
         const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        CHECK(*dictionary);
        return std::move(*dictionary);
      },
      base::Unretained(&dictionary));
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);
  std::move(dictionary_read_all_callback).Run(ERR_FAILED);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_DICTIONARY_LOAD_FAILED));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionaryFailureAfterStartReading) {
  scoped_refptr<DummyAsyncDictionary> dictionary =
      base::MakeRefCounted<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();

  MockHttpRequest request(kBrotliDictionaryTestTransaction);
  request.dictionary_getter = base::BindRepeating(
      [](scoped_refptr<DummyAsyncDictionary>* dictionary,
         const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        CHECK(*dictionary);
        return std::move(*dictionary);
      },
      base::Unretained(&dictionary));
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  std::move(dictionary_read_all_callback).Run(ERR_FAILED);

  EXPECT_EQ(ERR_DICTIONARY_LOAD_FAILED, read_callback.WaitForResult());
}

TEST_F(SharedDictionaryNetworkTransactionTest, Restart) {
  ScopedMockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.start_return_code = ERR_FAILED;
  MockHttpRequest request(mock_transaction);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return nullptr;
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(ERR_FAILED));

  {
    TestCompletionCallback restart_callback;
    ASSERT_THAT(
        transaction.RestartIgnoringLastError(restart_callback.callback()),
        test::IsError(ERR_FAILED));
  }
  {
    TestCompletionCallback restart_callback;
    ASSERT_THAT(
        transaction.RestartWithCertificate(
            /*client_cert=*/nullptr,
            /*client_private_key=*/nullptr, restart_callback.callback()),
        test::IsError(ERR_FAILED));
  }
  {
    TestCompletionCallback restart_callback;
    ASSERT_THAT(transaction.RestartWithAuth(AuthCredentials(),
                                            restart_callback.callback()),
                test::IsError(ERR_FAILED));
  }
  ASSERT_FALSE(transaction.IsReadyToRestartForAuth());
}

TEST_F(SharedDictionaryNetworkTransactionTest, StopCaching) {
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  EXPECT_FALSE(network_layer().stop_caching_called());
  transaction.StopCaching();
  EXPECT_TRUE(network_layer().stop_caching_called());
}

TEST_F(SharedDictionaryNetworkTransactionTest, DoneReading) {
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  EXPECT_FALSE(network_layer().done_reading_called());
  transaction.DoneReading();
  EXPECT_TRUE(network_layer().done_reading_called());
}

TEST_F(SharedDictionaryNetworkTransactionTest, GetLoadState) {
  ScopedMockTransaction scoped_mock_transaction(kSimpleGET_Transaction);
  MockHttpRequest request(scoped_mock_transaction);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return nullptr;
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  EXPECT_EQ(LOAD_STATE_IDLE, transaction.GetLoadState());

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(1);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, 1);

  EXPECT_EQ(LOAD_STATE_READING_RESPONSE, transaction.GetLoadState());
}

TEST_F(SharedDictionaryNetworkTransactionTest, SharedZstd) {
  base::HistogramTester histogram_tester;
  // Override MockTransaction to use `content-encoding: dcz`.
  scoped_mock_transaction_.reset();
  ScopedMockTransaction new_mock_transaction(kZstdDictionaryTestTransaction);

  MockHttpRequest request(new_mock_transaction);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/true);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));
  histogram_tester.ExpectUniqueSample(
      "Net.SharedDictionary.Transaction.Outcome",
      SharedDictionaryTransactionOutcome::kDictionaryUsedZstandard, 1);

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
#if defined(NET_DISABLE_ZSTD)
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_CONTENT_DECODING_FAILED));
#else   // defined(NET_DISABLE_ZSTD)
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
#endif  // defined(NET_DISABLE_ZSTD)
}

TEST_F(SharedDictionaryNetworkTransactionTest, NoZstdDContentEncoding) {
  // Change MockTransaction to remove `content-encoding: dcz`.
  scoped_mock_transaction_.reset();
  ScopedMockTransaction scoped_mock_transaction(kZstdDictionaryTestTransaction);
  scoped_mock_transaction.response_headers = "";

  MockHttpRequest request(scoped_mock_transaction);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/true);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is no "content-encoding: dcz" header,
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kZstdEncodedDataString.size());
  EXPECT_EQ(kZstdEncodedDataString, std::string(buf->data(), read_result));
}

enum class ProtocolCheckHostTestCase {
  kLocalHost,
  kNonLocalhost,
};
std::string ToString(ProtocolCheckHostTestCase host_type) {
  switch (host_type) {
    case ProtocolCheckHostTestCase::kLocalHost:
      return "LocalHost";
    case ProtocolCheckHostTestCase::kNonLocalhost:
      return "NonLocalhost";
  }
}

class SharedDictionaryNetworkTransactionProtocolCheckTest
    : public SharedDictionaryNetworkTransactionTest,
      public testing::WithParamInterface<ProtocolCheckHostTestCase> {
 public:
  SharedDictionaryNetworkTransactionProtocolCheckTest() {}
  SharedDictionaryNetworkTransactionProtocolCheckTest(
      const SharedDictionaryNetworkTransactionProtocolCheckTest&) = delete;
  SharedDictionaryNetworkTransactionProtocolCheckTest& operator=(
      const SharedDictionaryNetworkTransactionProtocolCheckTest&) = delete;
  ~SharedDictionaryNetworkTransactionProtocolCheckTest() override = default;

 protected:
  MockTransaction CreateMockTransaction() {
    MockTransaction mock_transaction = kBrotliDictionaryTestTransaction;
    if (IsLocalHost()) {
      mock_transaction.url = "http://localhost/test";
    }
    return mock_transaction;
  }

 private:
  bool IsLocalHost() const {
    return GetParam() == ProtocolCheckHostTestCase::kLocalHost;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedDictionaryNetworkTransactionProtocolCheckTest,
    testing::ValuesIn({ProtocolCheckHostTestCase::kLocalHost,
                       ProtocolCheckHostTestCase::kNonLocalhost}),
    [](const testing::TestParamInfo<ProtocolCheckHostTestCase>& info) {
      return ToString(info.param);
    });

TEST_P(SharedDictionaryNetworkTransactionProtocolCheckTest, Basic) {
  // Reset `scoped_mock_transaction_` to use the custom ScopedMockTransaction.
  scoped_mock_transaction_.reset();
  ScopedMockTransaction new_mock_transaction(CreateMockTransaction());

  MockHttpRequest request(new_mock_transaction);
  request.dictionary_getter = base::BindRepeating(
      [](const std::optional<SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<SharedDictionary> {
        return base::MakeRefCounted<DummySyncDictionary>(kTestDictionaryData);
      });
  SharedDictionaryNetworkTransaction transaction(CreateNetworkTransaction(),
                                                 /*enable_shared_zstd=*/false);
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                NetLogWithSource()),
              test::IsError(ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), test::IsError(OK));

  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(kDefaultBufferSize);
  TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      test::IsError(ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

}  // namespace

}  // namespace net
