// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_http_cache_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/test/test_with_task_environment.h"
#include "services/network/public/cpp/basic_data_buffer_factory.h"
#include "services/network/public/cpp/data_buffer_factory.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/shared_http_cache_client.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

std::string SerializeResponseInfo(const std::string& raw_headers) {
  net::HttpResponseInfo response_info;
  response_info.headers = net::HttpResponseHeaders::TryToCreate(raw_headers);
  CHECK(response_info.headers);
  response_info.request_time = base::Time::Now();
  response_info.response_time = base::Time::Now();
  auto pickle = response_info.MakePickle(/*skip_transient_headers=*/true,
                                         /*response_truncated=*/false);
  return std::string(pickle->AsStringView());
}

std::string GetStringFromBuffers(
    const std::unique_ptr<DataBufferList>& buffers) {
  if (!buffers) {
    return "";
  }
  std::string decoded_string;
  for (auto buffer : *buffers) {
    decoded_string.append(reinterpret_cast<const char*>(buffer.data()),
                          buffer.size());
  }
  return decoded_string;
}

}  // namespace

class SharedHttpCacheClientTest : public testing::Test,
                                  public net::WithTaskEnvironment {
 public:
  SharedHttpCacheClientTest() {
    AddScopedFeatureList().InitAndEnableFeature(
        net::features::kRendererAccessibleHttpCache);
  }
  ~SharedHttpCacheClientTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    factory_ = base::MakeRefCounted<BasicDataBufferFactory>();
  }

  void TearDown() override {
    // Flush `database_task_runner_` to ensure that any pending background tasks
    // are completed.
    base::test::TestFuture<void> future;
    database_task_runner_->PostTask(FROM_HERE,
                                    future.GetSequenceBoundCallback());
    EXPECT_TRUE(future.Wait());
  }

 protected:
  struct ResourceEntry {
    GURL url;
    std::string header_data;
    std::string body_data;
  };

  sqlite_vfs::PendingFileSet PopulateDatabase(
      const std::vector<ResourceEntry>& entries,
      int32_t db_id = 1) {
    disk_cache::SqlSharedCacheIsolatedDatabase db(
        "nik", temp_dir_.GetPath(), disk_cache::SqlSharedCacheDbId(db_id),
        database_task_runner_);
    EXPECT_TRUE(db.Init().has_value());

    for (const auto& entry : entries) {
      auto headers =
          base::MakeRefCounted<net::StringIOBuffer>(entry.header_data);
      disk_cache::CacheEntryKey key{base::StrCat({"0/0/", entry.url.spec()})};
      auto body = base::MakeRefCounted<net::StringIOBuffer>(entry.body_data);
      auto insert_result =
          db.Insert(key, headers, entry.body_data.size(), body);
      EXPECT_TRUE(insert_result.has_value());
    }

    auto pending_file_set = db.GetSharedReadOnlyConnection();
    EXPECT_TRUE(pending_file_set.has_value());
    return std::move(pending_file_set.value());
  }

  sqlite_vfs::PendingFileSet PopulateDatabase(const GURL& url,
                                              const std::string& header_data,
                                              const std::string& body_data,
                                              int32_t db_id = 1) {
    return PopulateDatabase({{url, header_data, body_data}}, db_id);
  }

  static ResourceRequest CreateRequest(const GURL& url,
                                       mojom::RequestDestination destination =
                                           mojom::RequestDestination::kScript) {
    ResourceRequest request;
    request.url = url;
    request.destination = destination;
    return request;
  }

  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;
  scoped_refptr<BasicDataBufferFactory> factory_;
};

TEST_F(SharedHttpCacheClientTest, ResponseMoveOperations) {
  auto head = mojom::URLResponseHead::New();
  head->mime_type = "text/html";
  auto body = factory_->CreateDataBufferList();
  body->Append(factory_->AllocateDataBuffer(10));

  SharedHttpCacheClient::Response response(std::move(head), std::move(body));
  EXPECT_TRUE(response.head);
  EXPECT_EQ(response.head->mime_type, "text/html");
  EXPECT_TRUE(response.body);

  SharedHttpCacheClient::Response moved_response(std::move(response));
  EXPECT_TRUE(moved_response.head);
  EXPECT_EQ(moved_response.head->mime_type, "text/html");
  EXPECT_TRUE(moved_response.body);
  EXPECT_FALSE(response.head);  // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(response.body);  // NOLINT(bugprone-use-after-move)

  SharedHttpCacheClient::Response assigned_response(nullptr, nullptr);
  assigned_response = std::move(moved_response);
  EXPECT_TRUE(assigned_response.head);
  EXPECT_TRUE(assigned_response.body);
  EXPECT_FALSE(moved_response.head);  // NOLINT(bugprone-use-after-move)
}

TEST_F(SharedHttpCacheClientTest, EarlyReturnWhenNotInitialized) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_);

  ResourceRequest request =
      CreateRequest(GURL("https://example.com/not_initialized.js"));

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(SharedHttpCacheClientTest, EarlyReturnOnIneligibleRequest) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/script.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\n\r\n");
  auto file_set = PopulateDatabase(url, headers, "content");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  // Ineligible destination (e.g. kDocument, kEmpty, kAudio).
  {
    auto request = CreateRequest(url, mojom::RequestDestination::kDocument);
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Take().has_value());
  }

  // Ineligible method.
  {
    auto request = CreateRequest(url);
    request.method = "POST";
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Take().has_value());
  }

  // Ineligible scheme (e.g. file:, http:).
  {
    for (const char* ineligible_url :
         {"file:///path/to/script.js", "http://example.com/script.js"}) {
      auto request = CreateRequest(GURL(ineligible_url));
      base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
          future;
      client->Find(request, factory_, future.GetCallback(),
                   base::SequencedTaskRunner::GetCurrentDefault());
      EXPECT_TRUE(future.IsReady());
      EXPECT_FALSE(future.Take().has_value());
    }
  }

  // Ineligible due to Range header.
  {
    auto request = CreateRequest(url);
    request.headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=0-10");
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Take().has_value());
  }

  // Ineligible due to LOAD_DISABLE_CACHE.
  {
    auto request = CreateRequest(url);
    request.load_flags = net::LOAD_DISABLE_CACHE;
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Take().has_value());
  }

  // Ineligible due to LOAD_BYPASS_CACHE.
  {
    auto request = CreateRequest(url);
    request.load_flags = net::LOAD_BYPASS_CACHE;
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Take().has_value());
  }

  // Ineligible due to LOAD_VALIDATE_CACHE.
  {
    auto request = CreateRequest(url);
    request.load_flags = net::LOAD_VALIDATE_CACHE;
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Take().has_value());
  }

  // Ineligible due to revalidation.
  {
    auto request = CreateRequest(url);
    request.is_revalidating = true;
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    EXPECT_FALSE(future.Take().has_value());
  }
}

TEST_F(SharedHttpCacheClientTest, EarlyReturnOnInvalidUrl) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_);

  ResourceRequest request =
      CreateRequest(GURL("https://example.com:999999/script.js"));
  ASSERT_FALSE(request.url.is_valid());

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(SharedHttpCacheClientTest, EarlyReturnWhenUrlNotInHashes) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL cached_url("https://example.com/cached.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  auto file_set = PopulateDatabase(cached_url, headers, "Hello World");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(cached_url.spec())});
  client_remote.FlushForTesting();

  // Request a URL that is not in the hash set.
  ResourceRequest request =
      CreateRequest(GURL("https://example.com/not_cached.js"));

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

// When CreateClient has been called but no hashes have been added yet (i.e.
// `OnResourcesAdded` has not been called), `ShouldEarlyReturn` returns false
// and `Find()` does not early return.
TEST_F(SharedHttpCacheClientTest, NoEarlyReturnWhenInitializedWithoutHashes) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL cached_url("https://example.com/cached.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  auto file_set = PopulateDatabase(cached_url, headers, "Hello World");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());

  ResourceRequest request =
      CreateRequest(GURL("https://example.com/no_hashes.js"));

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  // Since hashes have not been added, ShouldEarlyReturn returns false, so
  // Find() does not return synchronously.
  EXPECT_FALSE(future.IsReady());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

// Verifies that ThreadSafeSet uses base::LRUCacheSet with the specified
// maximum capacity, evicting the least-recently-used entry when new hashes
// are added, and that querying an entry refreshes its recency in the LRU order.
TEST_F(SharedHttpCacheClientTest, LruCacheSetEviction) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  constexpr size_t kMaxHashes = 2;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()),
      kMaxHashes);

  const GURL url1("https://example.com/1.js");
  const GURL url2("https://example.com/2.js");
  const GURL url3("https://example.com/3.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

  auto file_set = PopulateDatabase({
      {url1, headers, "body1"},
      {url2, headers, "body2"},
      {url3, headers, "body3"},
  });

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());

  // Add url1 and url2 to the cache filter.
  client_remote->OnResourcesAdded(
      {base::PersistentHash(url1.spec()), base::PersistentHash(url2.spec())});
  client_remote.FlushForTesting();

  // Query url1 to make it the most recently used in the LRU cache.
  // The recency order is now: url1 (MRU), url2 (LRU).
  {
    ResourceRequest request = CreateRequest(url1);
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    auto result = future.Take();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(GetStringFromBuffers(result->body), "body1");
  }

  // Add url3. Since kMaxHashes == 2, the least recently used entry (url2)
  // should be evicted from the LRU cache set.
  client_remote->OnResourcesAdded({base::PersistentHash(url3.spec())});
  client_remote.FlushForTesting();

  // url2 should now be evicted from the in-memory filter, causing Find() to
  // early-return nullopt synchronously even though url2 exists in the database.
  {
    ResourceRequest request = CreateRequest(url2);
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    auto result = future.Take();
    EXPECT_FALSE(result.has_value());
  }

  // url1 should still be present because it was accessed and made MRU.
  {
    ResourceRequest request = CreateRequest(url1);
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    auto result = future.Take();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(GetStringFromBuffers(result->body), "body1");
  }

  // url3 should also be present as the newest entry.
  {
    ResourceRequest request = CreateRequest(url3);
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    auto result = future.Take();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(GetStringFromBuffers(result->body), "body3");
  }
}

// Verifies that when entries in the LRU cache set are untouched, adding new
// entries evicts the oldest entries in FIFO order.
TEST_F(SharedHttpCacheClientTest, LruCacheSetFifoEvictionWhenUntouched) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  constexpr size_t kMaxHashes = 2;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()),
      kMaxHashes);

  const GURL url1("https://example.com/1.js");
  const GURL url2("https://example.com/2.js");
  const GURL url3("https://example.com/3.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");

  auto file_set = PopulateDatabase({
      {url1, headers, "body1"},
      {url2, headers, "body2"},
      {url3, headers, "body3"},
  });

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());

  // Add url1 and url2. Neither is accessed.
  client_remote->OnResourcesAdded(
      {base::PersistentHash(url1.spec()), base::PersistentHash(url2.spec())});
  client_remote.FlushForTesting();

  // Add url3 without touching url1 or url2. Since url1 was added first and
  // untouched, url1 should be evicted.
  client_remote->OnResourcesAdded({base::PersistentHash(url3.spec())});
  client_remote.FlushForTesting();

  // url1 is evicted -> synchronous early return nullopt.
  {
    ResourceRequest request = CreateRequest(url1);
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(future.IsReady());
    auto result = future.Take();
    EXPECT_FALSE(result.has_value());
  }

  // url2 was retained.
  {
    ResourceRequest request = CreateRequest(url2);
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    auto result = future.Take();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(GetStringFromBuffers(result->body), "body2");
  }

  // url3 was retained.
  {
    ResourceRequest request = CreateRequest(url3);
    base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>>
        future;
    client->Find(request, factory_, future.GetCallback(),
                 base::SequencedTaskRunner::GetCurrentDefault());
    auto result = future.Take();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(GetStringFromBuffers(result->body), "body3");
  }
}

TEST_F(SharedHttpCacheClientTest, FindSuccess) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/script.js");
  const std::string expected_body = "console.log('from shared cache');";
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\n\r\n");
  auto file_set = PopulateDatabase(url, headers, expected_body);

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  ResourceRequest request = CreateRequest(url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->head);
  EXPECT_TRUE(result->head->was_fetched_via_cache);
  EXPECT_EQ(result->head->headers->response_code(), 200);
  EXPECT_EQ(result->head->encoded_data_length,
            static_cast<int64_t>(result->head->headers->raw_headers().size() +
                                 expected_body.size()));
  EXPECT_EQ(GetStringFromBuffers(result->body), expected_body);
}

TEST_F(SharedHttpCacheClientTest, FindWithContentDecodingGzip) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/compressed.js");
  const std::string original_body = "function test() { return 42; }";
  std::vector<uint8_t> compressed =
      net::CompressGzip(original_body, /*gzip_framing=*/true);
  std::string compressed_body(compressed.begin(), compressed.end());

  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/javascript\r\n"
      "Content-Encoding: gzip\r\n\r\n");
  auto file_set = PopulateDatabase(url, headers, compressed_body);

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  ResourceRequest request = CreateRequest(url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(GetStringFromBuffers(result->body), original_body);
}

TEST_F(SharedHttpCacheClientTest, FindWithContentDecodingMultiChunkGzip) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/large_compressed.js");
  // Create a 100 KB payload exceeding the 64 KB default decoder buffer size.
  std::string original_body;
  original_body.reserve(100 * 1024);
  for (size_t i = 0; i < 10 * 1024; ++i) {
    original_body.append("0123456789");
  }
  std::vector<uint8_t> compressed =
      net::CompressGzip(original_body, /*gzip_framing=*/true);
  std::string compressed_body(compressed.begin(), compressed.end());

  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/javascript\r\n"
      "Content-Encoding: gzip\r\n\r\n");
  auto file_set = PopulateDatabase(url, headers, compressed_body);

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  ResourceRequest request = CreateRequest(url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  size_t chunk_count = 0;
  for ([[maybe_unused]] auto buffer : *result->body) {
    ++chunk_count;
  }
  EXPECT_GT(chunk_count, 1u);
  EXPECT_EQ(GetStringFromBuffers(result->body), original_body);
}

TEST_F(SharedHttpCacheClientTest, FindNotFoundInDb) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL cached_url("https://example.com/cached.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  auto file_set = PopulateDatabase(cached_url, headers, "Hello World");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());

  const GURL not_cached_url("https://example.com/not_in_db.js");
  // Add the hash to the set to pass ShouldEarlyReturn, simulating a hash
  // collision or an entry not present in the database.
  client_remote->OnResourcesAdded(
      {base::PersistentHash(not_cached_url.spec())});
  client_remote.FlushForTesting();

  ResourceRequest request = CreateRequest(not_cached_url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(SharedHttpCacheClientTest, MatchesUrlWithFragmentOrCredentials) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL cached_url("https://example.com/cached.js");
  const std::string expected_body = "cached body";
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  auto file_set = PopulateDatabase(cached_url, headers, expected_body);

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(cached_url.spec())});
  client_remote.FlushForTesting();

  // Request a URL that contains a fragment and credentials. The simplified URL
  // should match the cached URL hash, so it should not early return.
  ResourceRequest request;
  request.url = GURL("https://user:pass@example.com/cached.js#version=1");
  request.destination = mojom::RequestDestination::kScript;

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  // Since the simplified URL is in the hash set, ShouldEarlyReturn returns
  // false, so Find() does not return synchronously.
  EXPECT_FALSE(future.IsReady());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(SharedHttpCacheClientTest, FindResourceReadBodyFails) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/read_fail.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  auto file_set = PopulateDatabase(url, headers, "Hello World");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  class OversizedDataBufferFactory : public BasicDataBufferFactory {
   public:
    std::unique_ptr<DataBuffer> AllocateDataBuffer(uint32_t size) override {
      return BasicDataBufferFactory::AllocateDataBuffer(size + 1);
    }

   protected:
    ~OversizedDataBufferFactory() override = default;
  };
  auto oversized_factory = base::MakeRefCounted<OversizedDataBufferFactory>();

  ResourceRequest request = CreateRequest(url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, oversized_factory, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(SharedHttpCacheClientTest, ParseAndDecodeInvalidHeaders) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/invalid_headers.js");
  auto file_set =
      PopulateDatabase(url, "invalid_non_pickle_header_data", "body");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  ResourceRequest request = CreateRequest(url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(SharedHttpCacheClientTest, ParseAndDecodeDecompressionFails) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/corrupt_gzip.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/javascript\r\n"
      "Content-Encoding: gzip\r\n\r\n");
  auto file_set = PopulateDatabase(url, headers, "not valid gzip data");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  ResourceRequest request = CreateRequest(url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(SharedHttpCacheClientTest, FindEmptyBodyWithoutCompression) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/empty.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"
      "Content-Type: application/javascript\r\n\r\n");
  auto file_set = PopulateDatabase(url, headers, "");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  ResourceRequest request = CreateRequest(url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->head->content_length, 0);
  EXPECT_EQ(result->body->size(), 0u);
  EXPECT_EQ(GetStringFromBuffers(result->body), "");
}

TEST_F(SharedHttpCacheClientTest, ParseAndDecodeEmptyBodyWithCompression) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL url("https://example.com/empty_gzip.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/javascript\r\n"
      "Content-Encoding: gzip\r\n\r\n");
  // Empty body with gzip encoding.
  auto file_set = PopulateDatabase(url, headers, "");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(url.spec())});
  client_remote.FlushForTesting();

  ResourceRequest request = CreateRequest(url);

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(GetStringFromBuffers(result->body), "");
}

TEST_F(SharedHttpCacheClientTest, NoCircularDependency) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_);

  // Wait until DatabaseBackend is created and bound on database_task_runner_.
  base::test::TestFuture<void> future;
  database_task_runner_->PostTask(FROM_HERE, future.GetSequenceBoundCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(client->HasOneRef());

  // Disconnecting the Mojo pipe maintains single ownership.
  factory_remote.reset();
  base::test::TestFuture<void> disconnect_future;
  database_task_runner_->PostTask(FROM_HERE,
                                  disconnect_future.GetSequenceBoundCallback());
  ASSERT_TRUE(disconnect_future.Wait());
  EXPECT_TRUE(client->HasOneRef());

  client = nullptr;
}

TEST_F(SharedHttpCacheClientTest, DuplicateCreateClientReportsBadMessage) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL cached_url("https://example.com/cached.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  auto file_set = PopulateDatabase(cached_url, headers, "Hello World");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());

  base::test::TestFuture<std::string> bad_message_future;
  mojo::SetDefaultProcessErrorHandler(
      bad_message_future
          .GetSequenceBoundRepeatingCallback<const std::string&>());

  auto file_set2 =
      PopulateDatabase(cached_url, headers, "Hello World", /*db_id=*/2);
  mojo::Remote<mojom::SharedHttpCacheClient> second_client_remote;
  factory_remote->CreateClient(
      std::move(file_set2), second_client_remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ("Duplicate CreateClient call", bad_message_future.Take());

  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
}

TEST_F(SharedHttpCacheClientTest, DestructOnNonClientSequence) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL cached_url("https://example.com/cached.js");
  std::string headers = SerializeResponseInfo(
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  auto file_set = PopulateDatabase(cached_url, headers, "Hello World");

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(std::move(file_set),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote.FlushForTesting();

  // No circular reference exists; `client` holds the only reference.
  EXPECT_TRUE(client->HasOneRef());

  // Set up a disconnect handler on `client_remote` to verify that
  // `cache_client_` is destroyed when the final reference to `client` is
  // released.
  base::test::TestFuture<void> disconnect_future;
  client_remote.set_disconnect_handler(disconnect_future.GetCallback());

  // Release the final reference on `database_task_runner_`, which is not
  // `client_task_runner_`. In ~SharedHttpCacheClientImpl, `database_backend_`
  // is reset, which in turn resets `cache_client_` and posts its destruction
  // to `client_task_runner_`.
  base::test::TestFuture<void> destroyed_future;
  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<SharedHttpCacheClient> client,
             base::OnceClosure done) {
            client = nullptr;
            std::move(done).Run();
          },
          std::move(client), destroyed_future.GetSequenceBoundCallback()));
  EXPECT_TRUE(destroyed_future.Wait());

  // Deleting `cache_client_` on `client_task_runner_` closes the receiver and
  // triggers disconnection on `client_remote`.
  EXPECT_TRUE(disconnect_future.Wait());
}

}  // namespace network
