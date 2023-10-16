// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_network_transaction.h"

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

namespace network {

namespace {

const std::string kTestDictionaryData = "HelloHallo你好こんにちは";
// The hex of sha256 of `kTestDictionaryData`.
const std::string kTestDictionarySha256 =
    "c19728aed36503cfc81a0f5359e6f472e121f77bf20a2faac7994191293c0623";
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
  explicit DummySyncDictionary(const std::string& data_string)
      : data_(base::MakeRefCounted<net::StringIOBuffer>(data_string)),
        size_(data_string.size()) {
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

 private:
  const scoped_refptr<net::IOBuffer> data_;
  const size_t size_;
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
      const GURL& url) override {
    return std::move(dictionary_);
  }
  void GetDictionary(const GURL& url,
                     base::OnceCallback<void(std::unique_ptr<SharedDictionary>)>
                         callback) override {}
  scoped_refptr<SharedDictionaryWriter> CreateWriter(
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match) override {
    return nullptr;
  }
  bool IsAlreadyRegistered(const GURL& url,
                           base::Time response_time,
                           base::TimeDelta expiration,
                           const std::string& match) override {
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

static void BrotliTestTransactionHandler(const net::HttpRequestInfo* request,
                                         std::string* response_status,
                                         std::string* response_headers,
                                         std::string* response_data) {
  std::string sec_available_dictionary_header;
  EXPECT_TRUE(request->extra_headers.GetHeader(
      network::shared_dictionary::kSecAvailableDictionaryHeaderName,
      &sec_available_dictionary_header));
  EXPECT_EQ(kTestDictionarySha256, sec_available_dictionary_header);
  *response_data = kBrotliEncodedDataString;
}

static void ZstdTestTransactionHandler(const net::HttpRequestInfo* request,
                                       std::string* response_status,
                                       std::string* response_headers,
                                       std::string* response_data) {
  std::string sec_available_dictionary_header;
  EXPECT_TRUE(request->extra_headers.GetHeader(
      network::shared_dictionary::kSecAvailableDictionaryHeaderName,
      &sec_available_dictionary_header));
  EXPECT_EQ(kTestDictionarySha256, sec_available_dictionary_header);
  *response_data = kZstdEncodedDataString;
}

static void TestTransactionHandlerWithoutAvailableDictionary(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_FALSE(request->extra_headers.HasHeader(
      network::shared_dictionary::kSecAvailableDictionaryHeaderName));
  *response_data = kTestData;
}

const net::MockTransaction kBrotliDictionaryTestTransaction = {
    .url = "https://test.example/test",
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "",
    .load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = net::DefaultTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers = "content-encoding: sbr\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = absl::nullopt,
    .browser_run_id = absl::nullopt,
    .test_mode = net::TEST_MODE_NORMAL,
    .handler = BrotliTestTransactionHandler,
    .read_handler = nullptr,
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = net::OK,
    .read_return_code = net::OK,
};

const net::MockTransaction kZstdDictionaryTestTransaction = {
    .url = "https://test.example/test",
    .method = "GET",
    .request_time = base::Time(),
    .request_headers = "",
    .load_flags = net::LOAD_CAN_USE_SHARED_DICTIONARY,
    .transport_info = net::DefaultTransportInfo(),
    .status = "HTTP/1.1 200 OK",
    .response_headers = "content-encoding: zstd-d\n",
    .response_time = base::Time(),
    .data = "",  // We set the body in the `handler` function.
    .dns_aliases = {},
    .fps_cache_filter = absl::nullopt,
    .browser_run_id = absl::nullopt,
    .test_mode = net::TEST_MODE_NORMAL,
    .handler = ZstdTestTransactionHandler,
    .read_handler = nullptr,
    .cert = nullptr,
    .cert_status = 0,
    .ssl_connection_status = 0,
    .start_return_code = net::OK,
    .read_return_code = net::OK,
};

class SharedDictionaryNetworkTransactionTest : public ::testing::Test {
 public:
  SharedDictionaryNetworkTransactionTest()
      : network_layer_(std::make_unique<net::MockNetworkLayer>()) {
    net::AddMockTransaction(&kBrotliDictionaryTestTransaction);
  }
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

 private:
  std::unique_ptr<net::MockNetworkLayer> network_layer_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SharedDictionaryNetworkTransactionTest, SyncDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

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

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction = kBrotliDictionaryTestTransaction;
  new_mock_transaction.handler =
      TestTransactionHandlerWithoutAvailableDictionary;
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
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

TEST_F(SharedDictionaryNetworkTransactionTest, NoMatchingDictionary) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(nullptr));

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction = kBrotliDictionaryTestTransaction;
  new_mock_transaction.handler =
      TestTransactionHandlerWithoutAvailableDictionary;
  net::AddMockTransaction(&new_mock_transaction);

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

TEST_F(SharedDictionaryNetworkTransactionTest, OpaqueFrameOrigin) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction = kBrotliDictionaryTestTransaction;
  new_mock_transaction.handler =
      TestTransactionHandlerWithoutAvailableDictionary;
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
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

  // Override MockTransaction to check that there is no sec-available-dictionary
  // header.
  net::MockTransaction new_mock_transaction = kBrotliDictionaryTestTransaction;
  new_mock_transaction.handler =
      TestTransactionHandlerWithoutAvailableDictionary;
  net::AddMockTransaction(&new_mock_transaction);

  net::MockHttpRequest request(new_mock_transaction);
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

  // Override MockTransaction to remove `content-encoding: sbr`.
  net::MockTransaction new_mock_transaction = kBrotliDictionaryTestTransaction;
  new_mock_transaction.response_headers = "";
  net::AddMockTransaction(&new_mock_transaction);

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

  // When there is no "content-encoding: sbr" header,
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kBrotliEncodedDataString.size());
  EXPECT_EQ(kBrotliEncodedDataString, std::string(buf->data(), read_result));
}

TEST_F(SharedDictionaryNetworkTransactionTest, MultipleContentEncodingWithSbr) {
  DummySharedDictionaryManager manager(
      base::MakeRefCounted<DummySharedDictionaryStorage>(
          std::make_unique<DummySyncDictionary>(kTestDictionaryData)));

  // Override MockTransaction to set `content-encoding: sbr, deflate`.
  net::MockTransaction new_mock_transaction = kBrotliDictionaryTestTransaction;
  new_mock_transaction.response_headers = "content-encoding: sbr, deflate\n";
  net::AddMockTransaction(&new_mock_transaction);

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

  net::MockTransaction mock_transaction(net::kSimpleGET_Transaction);
  mock_transaction.start_return_code = net::ERR_FAILED;
  net::AddMockTransaction(&mock_transaction);
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

  net::AddMockTransaction(&net::kSimpleGET_Transaction);
  net::MockHttpRequest request(net::kSimpleGET_Transaction);
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
  net::MockTransaction new_mock_transaction = kZstdDictionaryTestTransaction;
  net::AddMockTransaction(&new_mock_transaction);

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

  // Override MockTransaction to remove `content-encoding: zstd-d`.
  net::MockTransaction new_mock_transaction = kZstdDictionaryTestTransaction;
  new_mock_transaction.response_headers = "";
  net::AddMockTransaction(&new_mock_transaction);

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

  // When there is no "content-encoding: zstd-d" header,
  // SharedDictionaryNetworkTransaction must not decode the body.
  EXPECT_THAT(read_result, kZstdEncodedDataString.size());
  EXPECT_EQ(kZstdEncodedDataString, std::string(buf->data(), read_result));
}

}  // namespace

}  // namespace network
