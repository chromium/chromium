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
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/features.h"
#include "net/test/test_with_task_environment.h"
#include "services/network/public/cpp/basic_data_buffer_factory.h"
#include "services/network/public/cpp/data_buffer_factory.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/shared_http_cache_client.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

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
    base::test::TestFuture<void> future;
    database_task_runner_->PostTask(FROM_HERE,
                                    future.GetSequenceBoundCallback());
    EXPECT_TRUE(future.Wait());
  }

 protected:
  sqlite_vfs::PendingFileSet CreateDummyFileSet() {
    base::FilePath file_path = temp_dir_.GetPath().AppendASCII("dummy.db");
    {
      base::File create_file(
          file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    }
    base::File db_file(file_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(db_file.IsValid());
    sqlite_vfs::PendingFileSet file_set;
    file_set.read_write = false;
    file_set.db_file = std::move(db_file);
    file_set.shared_lock = base::UnsafeSharedMemoryRegion::Create(64);
    return file_set;
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

  ResourceRequest request;
  request.url = GURL("https://example.com/not_initialized.js");

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

// TODO(crbug.com/473666511): Add tests for skipping other ineligible requests
// (e.g. non-GET HTTP methods, LOAD_DISABLE_CACHE, non-HTTP schemes) once
// implemented.
TEST_F(SharedHttpCacheClientTest, EarlyReturnOnRevalidationRequest) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_);

  ResourceRequest request;
  request.url = GURL("https://example.com/revalidate.js");
  request.is_revalidating = true;

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(SharedHttpCacheClientTest, EarlyReturnOnInvalidUrl) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_);

  ResourceRequest request;
  request.url = GURL("invalid-url");
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
  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(CreateDummyFileSet(),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(cached_url.spec())});
  client_remote.FlushForTesting();

  // Request a URL that is not in the hash set.
  ResourceRequest request;
  request.url = GURL("https://example.com/not_cached.js");

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

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(CreateDummyFileSet(),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());

  ResourceRequest request;
  request.url = GURL("https://example.com/no_hashes.js");

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  // Since hashes have not been added, ShouldEarlyReturn returns false, so
  // Find() does not return synchronously.
  EXPECT_FALSE(future.IsReady());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

// In this initial CL, the database lookup is not yet implemented. Even when the
// URL passes the in-memory hash check, Find() asynchronously returns
// std::nullopt. Database reading will be added in a follow-up CL.
TEST_F(SharedHttpCacheClientTest, FindReturnsNulloptWhenDbNotImplemented) {
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote;
  base::test::TestFuture<void> db_init_future;
  auto client = SharedHttpCacheClient::CreateAndInit(
      factory_remote.BindNewPipeAndPassReceiver(),
      base::SequencedTaskRunner::GetCurrentDefault(), database_task_runner_,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         db_init_future.GetCallback()));

  const GURL cached_url("https://example.com/cached.js");
  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(CreateDummyFileSet(),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(cached_url.spec())});
  client_remote.FlushForTesting();

  // Request a URL that is in the hash set.
  ResourceRequest request;
  request.url = cached_url;

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
  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(CreateDummyFileSet(),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());
  client_remote->OnResourcesAdded({base::PersistentHash(cached_url.spec())});
  client_remote.FlushForTesting();

  // Request a URL that contains a fragment and credentials. The simplified URL
  // should match the cached URL hash, so it should not early return.
  ResourceRequest request;
  request.url = GURL("https://user:pass@example.com/cached.js#version=1");

  base::test::TestFuture<std::optional<SharedHttpCacheClient::Response>> future;
  client->Find(request, factory_, future.GetCallback(),
               base::SequencedTaskRunner::GetCurrentDefault());

  // Since the simplified URL is in the hash set, ShouldEarlyReturn returns
  // false, so Find() does not return synchronously.
  EXPECT_FALSE(future.IsReady());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
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

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(CreateDummyFileSet(),
                               client_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(db_init_future.Wait());

  base::test::TestFuture<std::string> bad_message_future;
  mojo::SetDefaultProcessErrorHandler(
      bad_message_future
          .GetSequenceBoundRepeatingCallback<const std::string&>());

  mojo::Remote<mojom::SharedHttpCacheClient> second_client_remote;
  factory_remote->CreateClient(
      CreateDummyFileSet(), second_client_remote.BindNewPipeAndPassReceiver());
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

  mojo::Remote<mojom::SharedHttpCacheClient> client_remote;
  factory_remote->CreateClient(CreateDummyFileSet(),
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
