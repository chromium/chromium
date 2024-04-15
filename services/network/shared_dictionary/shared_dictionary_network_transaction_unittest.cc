// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_network_transaction.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_test_util.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "services/network/public/cpp/features.h"
#include "services/network/shared_dictionary/shared_dictionary.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

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
// $ brotli -o /tmp/out.sbr -D /tmp/dict /tmp/data
// $ xxd -i /tmp/out.sbr
const uint8_t kBrotliEncodedData[] = {0xa1, 0xe8, 0x01, 0x00, 0x22, 0x8d, 0x54,
                                      0xc6, 0xf6, 0x26, 0x81, 0x69, 0x46, 0x9d,
                                      0xb2, 0x60, 0x0e, 0x6b, 0xf5, 0x07, 0x02};
const std::string kBrotliEncodedDataString =
    std::string(reinterpret_cast<const char*>(kBrotliEncodedData),
                sizeof(kBrotliEncodedData));

// The zstd encoded data of `kTestData` using `kTestDictionaryData` as a
// dictionary.
// kZstdEncodedData is generated using the following commands:
// $ echo -n "HelloHallo你好こんにちは" > /tmp/dict
// $ echo -n "HelloこんにちはHallo你好HelloこんにちはHallo你好" > /tmp/data
// $ zstd -o /tmp/out.szst -D /tmp/dict /tmp/data
// $ xxd -i /tmp/out.szst
const uint8_t kZstdEncodedData[] = {
    0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x3e, 0x85, 0x00, 0x00, 0x28,
    0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x03, 0x00, 0x42, 0x35, 0x88,
    0x6a, 0x03, 0x87, 0x4c, 0x2d, 0xcd, 0x1e, 0xde, 0x25};
const std::string kZstdEncodedDataString =
    std::string(reinterpret_cast<const char*>(kZstdEncodedData),
                sizeof(kZstdEncodedData));

const size_t kDefaultBufferSize = 1023;

class DummySyncDictionary : public SharedDictionary {
 public:
  explicit DummySyncDictionary(const std::string& data_string,
                               const std::string& id = "")
      : data_(base::MakeRefCounted<net::StringIOBuffer>(data_string)),
        size_(data_string.size()),
        id_(id) {
    std::unique_ptr<crypto::SecureHash> secure_hash =
        crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    secure_hash->Update(data_->data(), size_);
    secure_hash->Finish(hash_.data, sizeof(hash_.data));
  }
  ~DummySyncDictionary() override = default;

  // SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override {
    return net::OK;
  }
  scoped_refptr<net::IOBuffer> data() const override { return data_; }
  size_t size() const override { return size_; }
  const net::SHA256HashValue& hash() const override { return hash_; }
  const std::string& id() const override { return id_; }

 private:
  const scoped_refptr<net::IOBuffer> data_;
  const size_t size_;
  const std::string id_;
  net::SHA256HashValue hash_;
};

class DummyAsyncDictionary : public DummySyncDictionary {
 public:
  explicit DummyAsyncDictionary(const std::string& data_string)
      : DummySyncDictionary(data_string) {}
  ~DummyAsyncDictionary() override = default;

  // SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override {
    read_all_callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }
  base::OnceCallback<void(int)> TakeReadAllCallback() {
    return std::move(read_all_callback_);
  }

 private:
  base::OnceCallback<void(int)> read_all_callback_;
};

class DummySharedDictionaryStorage : public SharedDictionaryStorage {
 public:
  explicit DummySharedDictionaryStorage(
      std::unique_ptr<SharedDictionary> dictionary)
      : dictionary_(std::move(dictionary)) {}

  // SharedDictionaryStorage
  std::unique_ptr<SharedDictionary> GetDictionarySync(
      const GURL& url,
      mojom::RequestDestination destination) override {
    return std::move(dictionary_);
  }
  void GetDictionary(const GURL& url,
                     mojom::RequestDestination destination,
                     base::OnceCallback<void(std::unique_ptr<SharedDictionary>)>
                         callback) override {}
  scoped_refptr<SharedDictionaryWriter> CreateWriter(
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match,
      const std::set<mojom::RequestDestination>& match_dest,
      const std::string& id) override {
    return nullptr;
  }
  bool IsAlreadyRegistered(
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match,
      const std::set<mojom::RequestDestination>& match_dest,
      const std::string& id) override {
    return false;
  }

  void set_on_deleted_closure_runner(base::ScopedClosureRunner closure_runner) {
    on_deleted_closure_runner_ = std::move(closure_runner);
  }

 private:
  ~DummySharedDictionaryStorage() override = default;

  std::unique_ptr<SharedDictionary> dictionary_;
  base::ScopedClosureRunner on_deleted_closure_runner_;
};

class DummySharedDictionaryManager : public SharedDictionaryManager {
 public:
  explicit DummySharedDictionaryManager(
      scoped_refptr<DummySharedDictionaryStorage> storage)
      : storage_(std::move(storage)) {}
  ~DummySharedDictionaryManager() override = default;

  scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryIsolationKey& isolation_key) override {
    create_storage_called_ = true;
    if (storage_) {
      storage_->set_on_deleted_closure_runner(base::ScopedClosureRunner(
          base::BindOnce(&SharedDictionaryManager::OnStorageDeleted,
                         GetWeakPtr(), isolation_key)));
    }
    return storage_;
  }
  void SetCacheMaxSize(uint64_t cache_max_size) override {}
  void ClearData(base::Time start_time,
                 base::Time end_time,
                 base::RepeatingCallback<bool(const GURL&)> url_matcher,
                 base::OnceClosure callback) override {}
  void ClearDataForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceClosure callback) override {}
  void GetUsageInfo(base::OnceCallback<
                    void(const std::vector<net::SharedDictionaryUsageInfo>&)>
                        callback) override {}
  void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceCallback<
          void(std::vector<network::mojom::SharedDictionaryInfoPtr>)> callback)
      override {}
  void GetOriginsBetween(
      base::Time start_time,
      base::Time end_time,
      base::OnceCallback<void(const std::vector<url::Origin>&)> callback)
      override {}

  bool create_storage_called() const { return create_storage_called_; }

 private:
  scoped_refptr<DummySharedDictionaryStorage> storage_;
  bool create_storage_called_ = false;
};

net::TransportInfo TestSpdyTransportInfo() {
  return net::TransportInfo(
      net::TransportType::kDirect,
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), 80),
      /*accept_ch_frame_arg=*/"",
      /*cert_is_issued_by_known_root=*/false, net::kProtoHTTP2);
}

static void BrotliTestTransactionHandler(const net::HttpRequestInfo* request,
                                         std::string* response_status,
                                         std::string* response_headers,
                                         std::string* response_data) {
  std::string header_value;
  EXPECT_TRUE(request->extra_headers.GetHeader(
      network::shared_dictionary::kAvailableDictionaryHeaderName,
      &header_value));
  EXPECT_EQ(kTestDictionarySha256Base64, header_value);
  *response_data = kBrotliEncodedDataString;
}

static void ZstdTestTransactionHandler(const net::HttpRequestInfo* request,
                                       std::string* response_status,
                                       std::string* response_headers,
                                       std::string* response_data) {
  std::string header_value;
      EXPECT_TRUE(request->extra_headers.GetHeader(
          network::shared_dictionary::kAvailableDictionaryHeaderName,
          &header_value));
      EXPECT_EQ(kTestDictionarySha256Base64, header_value);
  *response_data = kZstdEncodedDataString;
}

static const auto kTestTransactionHandlerWithoutAvailableDictionary =
    base::BindRepeating([](const net::HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data) {
      EXPECT_FALSE(request->extra_headers.HasHeader(
          network::shared_dictionary::kAvailableDictionaryHeaderName));
      *response_data = kTestData;
    });

constexpr char kTestUrl[] = "https://test.example/test";

const net::MockTransaction kBrotliDictionaryTestTransaction = {
    .url = kTestUrl,
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "sec-fetch-dest: document\r\n",
    .load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = TestSpdyTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers =
        "content-encoding: br-d\n"
        "content-dictionary: :wZcortNlA8/IGg9TWeb0cuEh93vyCi+qx5lBkSk8BiM=:\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = std::nullopt,
    .browser_run_id = std::nullopt,
    .test_mode = net::TEST_MODE_NORMAL,
    .handler = base::BindRepeating(&BrotliTestTransactionHandler),
    .read_handler = net::MockTransactionReadHandler(),
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = net::OK,
    .read_return_code = net::OK,
};

const net::MockTransaction kZstdDictionaryTestTransaction = {
    .url = kTestUrl,
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "sec-fetch-dest: document\r\n",
    .load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = TestSpdyTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers =
        "content-encoding: zstd-d\n"
        "content-dictionary: :wZcortNlA8/IGg9TWeb0cuEh93vyCi+qx5lBkSk8BiM=:\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = std::nullopt,
    .browser_run_id = std::nullopt,
    .test_mode = net::TEST_MODE_NORMAL,
    .handler = base::BindRepeating(&ZstdTestTransactionHandler),
    .read_handler = net::MockTransactionReadHandler(),
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = net::OK,
    .read_return_code = net::OK,
};

class SharedDictionaryNetworkTransactionTest : public ::testing::Test {
 public:
  SharedDictionaryNetworkTransactionTest()
      : scoped_mock_transaction_(kBrotliDictionaryTestTransaction),
        network_layer_(std::make_unique<net::MockNetworkLayer>()) {}
  ~SharedDictionaryNetworkTransactionTest() override = default;

  SharedDictionaryNetworkTransactionTest(
      const SharedDictionaryNetworkTransactionTest&) = delete;
  SharedDictionaryNetworkTransactionTest& operator=(
      const SharedDictionaryNetworkTransactionTest&) = delete;

 protected:
  std::unique_ptr<net::HttpTransaction> CreateNetworkTransaction() {
    std::unique_ptr<net::HttpTransaction> network_transaction;
    network_layer_->CreateTransaction(net::DEFAULT_PRIORITY,
                                      &network_transaction);
    return network_transaction;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  net::MockNetworkLayer& network_layer() { return *network_layer_.get(); }

  std::optional<net::ScopedMockTransaction> scoped_mock_transaction_;

 private:
  std::unique_ptr<net::MockNetworkLayer> network_layer_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SharedDictionaryNetworkTransactionTest, SyncDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, NotAllowedToUseDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return false; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, DictionaryId) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData,
                                                "test-id")));

  // Change MockTransaction to check the dictionary-id header
  scoped_mock_transaction_->handler = base::BindRepeating(
      [](const net::HttpRequestInfo* request, std::string* response_status,
         std::string* response_headers, std::string* response_data) {
        std::string dictionary_id;
        EXPECT_TRUE(
            request->extra_headers.GetHeader("dictionary-id", &dictionary_id));
        EXPECT_EQ("\"test-id\"", dictionary_id);
        *response_data = kBrotliEncodedDataString;
      });

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       DictionaryIdWithBackSlashAndDquote) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData,
                                                "test\\dictionary\"id")));

  // Change MockTransaction to check the dictionary-id header
  scoped_mock_transaction_->handler = base::BindRepeating(
      [](const net::HttpRequestInfo* request, std::string* response_status,
         std::string* response_headers, std::string* response_data) {
        std::string dictionary_id;
        EXPECT_TRUE(
            request->extra_headers.GetHeader("dictionary-id", &dictionary_id));
        EXPECT_EQ("\"test\\\\dictionary\\\"id\"", dictionary_id);
        *response_data = kBrotliEncodedDataString;
      });

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, EmptyDictionaryId) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData, "")));

  // Change MockTransaction to check the dictionary-id header
  scoped_mock_transaction_->handler = base::BindRepeating(
      [](const net::HttpRequestInfo* request, std::string* response_status,
         std::string* response_headers, std::string* response_data) {
        EXPECT_FALSE(request->extra_headers.HasHeader("dictionary-id"));
        *response_data = kBrotliEncodedDataString;
      });

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kCompressionDictionaryTransportRequireKnownRootCert);
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;
  scoped_mock_transaction_->transport_info.cert_is_issued_by_known_root = false;

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckSuccess) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kCompressionDictionaryTransportRequireKnownRootCert);
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // The BrotliTestTransactionHandler `scoped_mock_transaction_->handler` will
  // check that the there is a correct available-dictionary request header.
  scoped_mock_transaction_->transport_info.cert_is_issued_by_known_root = true;

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       RequireKnownRootCertCheckSuccessForLocalhost) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kCompressionDictionaryTransportRequireKnownRootCert);
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // The BrotliTestTransactionHandler `new_mock_transaction.handler` will check
  // that the there is a correct available-dictionary request header.
  net::ScopedMockTransaction scoped_mock_transaction(
      kBrotliDictionaryTestTransaction, "http:///localhost:1234/test");
  scoped_mock_transaction.transport_info.cert_is_issued_by_known_root = false;

  net::MockHttpRequest request(scoped_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, NoMatchingDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, OpaqueFrameOrigin) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;

  net::MockHttpRequest request(*scoped_mock_transaction_);
  request.frame_origin = url::Origin();
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, WithoutValidLoadFlag) {
  DummySharedDictionaryManager manager(/*storage=*/nullptr);

  // Change MockTransaction to check that there is no available-dictionary
  // header.
  scoped_mock_transaction_->handler =
      kTestTransactionHandlerWithoutAvailableDictionary;

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());

  CHECK_EQ(net::LOAD_CAN_USE_SHARED_DICTIONARY, request.load_flags);
  // Change load_flags not to trigger the shared dictionary logic.
  request.load_flags = net::LOAD_NORMAL;

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));

  // SharedDictionaryManager::CreateStorage() must not be called when
  // LOAD_CAN_USE_SHARED_DICTIONARY is not set.
  EXPECT_FALSE(manager.create_storage_called());
}

TEST_F(SharedDictionaryNetworkTransactionTest, NoSbrContentEncoding) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Change MockTransaction to remove `content-encoding: sbr`.
  scoped_mock_transaction_->response_headers = "";

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is no "content-encoding: sbr" header,
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kBrotliEncodedDataString.size());
  EXPECT_EQ(kBrotliEncodedDataString, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, NoContentDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Change MockTransaction to remove "content-dictionary" header.
  scoped_mock_transaction_->response_headers = "content-encoding: br-d\n";

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(
      start_callback.WaitForResult(),
      net::test::IsError(net::ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER));
}

TEST_F(SharedDictionaryNetworkTransactionTest, WrongContentDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Change MockTransaction to change the "content-dictionary" header.
  // The hash `kTestDictionaryData` is
  // ":wZcortNlA8/IGg9TWeb0cuEh93vyCi+qx5lBkSk8BiM=:". But the header contains
  // "content-dictionary" header with a different hash
  // ":U5abz16WDg7b8KS93msLPpOB4Vbef1uRzoORYkJw9BY=:".
  scoped_mock_transaction_->response_headers =
      "content-encoding: br-d\n"
      "content-dictionary: "
      ":U5abz16WDg7b8KS93msLPpOB4Vbef1uRzoORYkJw9BY=:\n";

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(
      start_callback.WaitForResult(),
      net::test::IsError(net::ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER));
}

TEST_F(SharedDictionaryNetworkTransactionTest, MultipleContentEncodingWithSbr) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Change MockTransaction to set `content-encoding: sbr, deflate`.
  scoped_mock_transaction_->response_headers =
      "content-encoding: sbr, deflate\n";

  net::MockHttpRequest request(*scoped_mock_transaction_);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is Content-Encoding header which value is other than "sbr",
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kBrotliEncodedDataString.size());
  EXPECT_EQ(kBrotliEncodedDataString, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessBeforeStartReading) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(kBrotliDictionaryTestTransaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);
  std::move(dictionary_read_all_callback).Run(net::OK);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessAfterStartReading) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(kBrotliDictionaryTestTransaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  std::move(dictionary_read_all_callback).Run(net::OK);

  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionarySuccessAfterTransactionDestroy) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(kBrotliDictionaryTestTransaction);
  std::unique_ptr<SharedDictionaryNetworkTransaction> transaction =
      std::make_unique<SharedDictionaryNetworkTransaction>(
          manager, CreateNetworkTransaction());
  transaction->SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction->Start(&request, start_callback.callback(),
                                 net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction->Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  transaction.reset();

  std::move(dictionary_read_all_callback).Run(net::OK);

  EXPECT_FALSE(read_callback.have_result());
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionaryFailureBeforeStartReading) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(kBrotliDictionaryTestTransaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);
  std::move(dictionary_read_all_callback).Run(net::ERR_FAILED);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_DICTIONARY_LOAD_FAILED));
}

TEST_F(SharedDictionaryNetworkTransactionTest,
       AsyncDictionaryFailureAfterStartReading) {
  std::unique_ptr<DummyAsyncDictionary> dictionary =
      std::make_unique<DummyAsyncDictionary>(kTestDictionaryData);
  DummyAsyncDictionary* dictionary_ptr = dictionary.get();
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::move(dictionary)));

  net::MockHttpRequest request(kBrotliDictionaryTestTransaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  base::OnceCallback<void(int)> dictionary_read_all_callback =
      dictionary_ptr->TakeReadAllCallback();
  ASSERT_TRUE(dictionary_read_all_callback);

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  std::move(dictionary_read_all_callback).Run(net::ERR_FAILED);

  EXPECT_EQ(net::ERR_DICTIONARY_LOAD_FAILED, read_callback.WaitForResult());
}

TEST_F(SharedDictionaryNetworkTransactionTest, Restart) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  net::ScopedMockTransaction mock_transaction(net::kSimpleGET_Transaction);
  mock_transaction.start_return_code = net::ERR_FAILED;
  net::MockHttpRequest request(mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(),
              net::test::IsError(net::ERR_FAILED));

  {
    net::TestCompletionCallback restart_callback;
    ASSERT_THAT(
        transaction.RestartIgnoringLastError(restart_callback.callback()),
        net::test::IsError(net::ERR_FAILED));
  }
  {
    net::TestCompletionCallback restart_callback;
    ASSERT_THAT(
        transaction.RestartWithCertificate(
            /*client_cert=*/nullptr,
            /*client_private_key=*/nullptr, restart_callback.callback()),
        net::test::IsError(net::ERR_FAILED));
  }
  {
    net::TestCompletionCallback restart_callback;
    ASSERT_THAT(transaction.RestartWithAuth(net::AuthCredentials(),
                                            restart_callback.callback()),
                net::test::IsError(net::ERR_FAILED));
  }
  ASSERT_FALSE(transaction.IsReadyToRestartForAuth());
}

TEST_F(SharedDictionaryNetworkTransactionTest, StopCaching) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  EXPECT_FALSE(network_layer().stop_caching_called());
  transaction.StopCaching();
  EXPECT_TRUE(network_layer().stop_caching_called());
}

TEST_F(SharedDictionaryNetworkTransactionTest, DoneReading) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  EXPECT_FALSE(network_layer().done_reading_called());
  transaction.DoneReading();
  EXPECT_TRUE(network_layer().done_reading_called());
}

TEST_F(SharedDictionaryNetworkTransactionTest, GetLoadState) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  net::ScopedMockTransaction scoped_mock_transaction(
      net::kSimpleGET_Transaction);
  net::MockHttpRequest request(scoped_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  EXPECT_EQ(net::LOAD_STATE_IDLE, transaction.GetLoadState());

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(1);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, 1);

  EXPECT_EQ(net::LOAD_STATE_READING_RESPONSE, transaction.GetLoadState());
}

TEST_F(SharedDictionaryNetworkTransactionTest, SharedZstd) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(network::features::kSharedZstd);

  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to use `content-encoding: zstd-d`.
  scoped_mock_transaction_.reset();
  net::ScopedMockTransaction new_mock_transaction(
      kZstdDictionaryTestTransaction);

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, NoZstdDContentEncoding) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(network::features::kSharedZstd);

  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Change MockTransaction to remove `content-encoding: zstd-d`.
  scoped_mock_transaction_.reset();
  net::ScopedMockTransaction scoped_mock_transaction(
      kZstdDictionaryTestTransaction);
  scoped_mock_transaction.response_headers = "";

  net::MockHttpRequest request(scoped_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();

  // When there is no "content-encoding: zstd-d" header,
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kZstdEncodedDataString.size());
  EXPECT_EQ(kZstdEncodedDataString, std::string(buf->data(), read_result));
}

enum class ProtocolCheckProtocolTestCase {
  kHttp1,
  kHttp2,
  kHttp3,
};
std::string ToString(ProtocolCheckProtocolTestCase protocol) {
  switch (protocol) {
    case ProtocolCheckProtocolTestCase::kHttp1:
      return "Http1";
    case ProtocolCheckProtocolTestCase::kHttp2:
      return "Http2";
    case ProtocolCheckProtocolTestCase::kHttp3:
      return "Http3";
  }
}

enum class ProtocolCheckHttp1TestCase {
  kAllowHttp1,
  kDoNotAllowHttp1,
};
std::string ToString(ProtocolCheckHttp1TestCase feature) {
  switch (feature) {
    case ProtocolCheckHttp1TestCase::kAllowHttp1:
      return "AllowHttp1";
    case ProtocolCheckHttp1TestCase::kDoNotAllowHttp1:
      return "DoNotAllowHttp1";
  }
}

enum class ProtocolCheckHttp2TestCase {
  kAllowHttp2,
  kDoNotAllowHttp2,
};
std::string ToString(ProtocolCheckHttp2TestCase feature) {
  switch (feature) {
    case ProtocolCheckHttp2TestCase::kAllowHttp2:
      return "AllowHttp2";
    case ProtocolCheckHttp2TestCase::kDoNotAllowHttp2:
      return "DoNotAllowHttp2";
  }
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
      public testing::WithParamInterface<
          std::tuple<ProtocolCheckHttp1TestCase,
                     ProtocolCheckHttp2TestCase,
                     ProtocolCheckProtocolTestCase,
                     ProtocolCheckHostTestCase>> {
 public:
  SharedDictionaryNetworkTransactionProtocolCheckTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (AllowHttp1()) {
      enabled_features.push_back(
          network::features::kCompressionDictionaryTransportOverHttp1);
    } else {
      disabled_features.push_back(
          network::features::kCompressionDictionaryTransportOverHttp1);
    }
    if (AllowHttp2()) {
      enabled_features.push_back(
          network::features::kCompressionDictionaryTransportOverHttp2);
    } else {
      disabled_features.push_back(
          network::features::kCompressionDictionaryTransportOverHttp2);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  SharedDictionaryNetworkTransactionProtocolCheckTest(
      const SharedDictionaryNetworkTransactionProtocolCheckTest&) = delete;
  SharedDictionaryNetworkTransactionProtocolCheckTest& operator=(
      const SharedDictionaryNetworkTransactionProtocolCheckTest&) = delete;
  ~SharedDictionaryNetworkTransactionProtocolCheckTest() override = default;

 protected:
  net::MockTransaction CreateMockTransaction() {
    net::MockTransaction mock_transaction = kBrotliDictionaryTestTransaction;
    if (IsLocalHost()) {
      mock_transaction.url = "http://localhost/test";
    }
    if (!ShuoldUseDictionary()) {
      // Change MockTransaction to check that there is no available-dictionary
      // header.
      mock_transaction.handler =
          kTestTransactionHandlerWithoutAvailableDictionary;
    }
    if (IsHttp2()) {
      mock_transaction.transport_info.negotiated_protocol = net::kProtoHTTP2;
    } else if (IsHttp3()) {
      mock_transaction.transport_info.negotiated_protocol = net::kProtoQUIC;
    } else {
      mock_transaction.transport_info.negotiated_protocol = net::kProtoHTTP11;
    }
    return mock_transaction;
  }

 private:
  bool AllowHttp1() const {
    return std::get<0>(GetParam()) == ProtocolCheckHttp1TestCase::kAllowHttp1;
  }
  bool AllowHttp2() const {
    return std::get<1>(GetParam()) == ProtocolCheckHttp2TestCase::kAllowHttp2;
  }
  bool IsHttp1() const {
    return std::get<2>(GetParam()) == ProtocolCheckProtocolTestCase::kHttp1;
  }
  bool IsHttp2() const {
    return std::get<2>(GetParam()) == ProtocolCheckProtocolTestCase::kHttp2;
  }
  bool IsHttp3() const {
    return std::get<2>(GetParam()) == ProtocolCheckProtocolTestCase::kHttp3;
  }
  bool IsLocalHost() const {
    return std::get<3>(GetParam()) == ProtocolCheckHostTestCase::kLocalHost;
  }
  bool ShuoldUseDictionary() const {
    if (AllowHttp1()) {
      if (AllowHttp2()) {
        return true;
      } else {
        return IsLocalHost() || IsHttp1() || IsHttp3();
      }
    } else {
      if (AllowHttp2()) {
        return IsLocalHost() || IsHttp2() || IsHttp3();
      } else {
        return IsLocalHost() || IsHttp3();
      }
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedDictionaryNetworkTransactionProtocolCheckTest,
    ::testing::Combine(
        ::testing::Values(ProtocolCheckHttp1TestCase::kAllowHttp1,
                          ProtocolCheckHttp1TestCase::kDoNotAllowHttp1),
        ::testing::Values(ProtocolCheckHttp2TestCase::kAllowHttp2,
                          ProtocolCheckHttp2TestCase::kDoNotAllowHttp2),
        ::testing::Values(ProtocolCheckProtocolTestCase::kHttp1,
                          ProtocolCheckProtocolTestCase::kHttp2,
                          ProtocolCheckProtocolTestCase::kHttp3),
        ::testing::Values(ProtocolCheckHostTestCase::kLocalHost,
                          ProtocolCheckHostTestCase::kNonLocalhost)),
    [](const testing::TestParamInfo<std::tuple<ProtocolCheckHttp1TestCase,
                                               ProtocolCheckHttp2TestCase,
                                               ProtocolCheckProtocolTestCase,
                                               ProtocolCheckHostTestCase>>&
           info) {
      return ToString(std::get<0>(info.param)) + "_" +
             ToString(std::get<1>(info.param)) + "_" +
             ToString(std::get<2>(info.param)) + "_" +
             ToString(std::get<3>(info.param));
    });

TEST_P(SharedDictionaryNetworkTransactionProtocolCheckTest, Basic) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Reset `scoped_mock_transaction_` to use the custom ScopedMockTransaction.
  scoped_mock_transaction_.reset();
  net::ScopedMockTransaction new_mock_transaction(CreateMockTransaction());

  net::MockHttpRequest request(new_mock_transaction);
  SharedDictionaryNetworkTransaction transaction(manager,
                                                 CreateNetworkTransaction());
  transaction.SetIsSharedDictionaryReadAllowedCallback(
      base::BindRepeating([]() { return true; }));

  net::TestCompletionCallback start_callback;
  ASSERT_THAT(transaction.Start(&request, start_callback.callback(),
                                net::NetLogWithSource()),
              net::test::IsError(net::ERR_IO_PENDING));
  EXPECT_THAT(start_callback.WaitForResult(), net::test::IsError(net::OK));

  scoped_refptr<net::IOBufferWithSize> buf =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultBufferSize);
  net::TestCompletionCallback read_callback;
  ASSERT_THAT(
      transaction.Read(buf.get(), buf->size(), read_callback.callback()),
      net::test::IsError(net::ERR_IO_PENDING));
  int read_result = read_callback.WaitForResult();
  EXPECT_THAT(read_result, kTestData.size());
  EXPECT_EQ(kTestData, std::string(buf->data(), read_result));
}

}  // namespace

}  // namespace network
