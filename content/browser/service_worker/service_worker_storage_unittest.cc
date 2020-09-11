// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_message.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

using net::IOBuffer;
using net::TestCompletionCallback;
using net::WrappedIOBuffer;

// TODO(crbug.com/1055677): Move out tests that rely on
// ServiceWorkerRegistry and put them in a separate unittest file.

namespace content {
namespace service_worker_storage_unittest {

struct ReadResponseHeadResult {
  int result;
  network::mojom::URLResponseHeadPtr response_head;
  base::Optional<mojo_base::BigBuffer> metadata;
};

using RegistrationData = storage::mojom::ServiceWorkerRegistrationData;
using ResourceRecord = storage::mojom::ServiceWorkerResourceRecordPtr;

ResourceRecord CreateResourceRecord(int64_t resource_id,
                                    const GURL& url,
                                    int64_t size_bytes) {
  EXPECT_TRUE(url.is_valid());
  return storage::mojom::ServiceWorkerResourceRecord::New(resource_id, url,
                                                          size_bytes);
}

void StatusCallback(base::OnceClosure quit_closure,
                    base::Optional<blink::ServiceWorkerStatusCode>* result,
                    blink::ServiceWorkerStatusCode status) {
  *result = status;
  std::move(quit_closure).Run();
}

void DatabaseStatusCallback(
    base::OnceClosure quit_closure,
    base::Optional<ServiceWorkerDatabase::Status>* result,
    ServiceWorkerDatabase::Status status) {
  *result = status;
  std::move(quit_closure).Run();
}

void FindCallback(base::OnceClosure quit_closure,
                  base::Optional<blink::ServiceWorkerStatusCode>* result,
                  scoped_refptr<ServiceWorkerRegistration>* found,
                  blink::ServiceWorkerStatusCode status,
                  scoped_refptr<ServiceWorkerRegistration> registration) {
  *result = status;
  *found = std::move(registration);
  std::move(quit_closure).Run();
}

void UserDataCallback(
    base::OnceClosure quit,
    std::vector<std::string>* data_out,
    base::Optional<blink::ServiceWorkerStatusCode>* status_out,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  *data_out = data;
  *status_out = status;
  std::move(quit).Run();
}

int WriteResponse(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    const std::string& headers,
    mojo_base::BigBuffer body) {
  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer;
  storage->CreateResourceWriter(id, writer.BindNewPipeAndPassReceiver());

  int rv = 0;
  {
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->request_time = base::Time::Now();
    response_head->response_time = base::Time::Now();
    response_head->headers = new net::HttpResponseHeaders(headers);
    response_head->content_length = body.size();

    base::RunLoop loop;
    writer->WriteResponseHead(std::move(response_head),
                              base::BindLambdaForTesting([&](int result) {
                                rv = result;
                                loop.Quit();
                              }));
    loop.Run();
    if (rv < 0)
      return rv;
  }

  {
    base::RunLoop loop;
    writer->WriteData(std::move(body),
                      base::BindLambdaForTesting([&](int result) {
                        rv = result;
                        loop.Quit();
                      }));
    loop.Run();
  }

  return rv;
}

int WriteStringResponse(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    const std::string& headers,
    const std::string& body) {
  mojo_base::BigBuffer buffer(
      base::as_bytes(base::make_span(body.data(), body.length())));
  return WriteResponse(storage, id, headers, std::move(buffer));
}

int WriteBasicResponse(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id) {
  const char kHttpHeaders[] = "HTTP/1.0 200 HONKYDORY\0Content-Length: 5\0\0";
  const char kHttpBody[] = "Hello";
  std::string headers(kHttpHeaders, base::size(kHttpHeaders));
  return WriteStringResponse(storage, id, headers, std::string(kHttpBody));
}

ReadResponseHeadResult ReadResponseHead(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id) {
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
  storage->CreateResourceReader(id, reader.BindNewPipeAndPassReceiver());

  ReadResponseHeadResult out;
  base::RunLoop loop;
  reader->ReadResponseHead(base::BindLambdaForTesting(
      [&](int result, network::mojom::URLResponseHeadPtr response_head,
          base::Optional<mojo_base::BigBuffer> metadata) {
        out.result = result;
        out.response_head = std::move(response_head);
        out.metadata = std::move(metadata);
        loop.Quit();
      }));
  loop.Run();
  return out;
}

bool VerifyBasicResponse(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    bool expected_positive_result) {
  const std::string kExpectedHttpBody("Hello");
  ReadResponseHeadResult out = ReadResponseHead(storage, id);
  if (expected_positive_result)
    EXPECT_LT(0, out.result);
  if (out.result <= 0)
    return false;

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
  storage->CreateResourceReader(id, reader.BindNewPipeAndPassReceiver());

  const int kBigEnough = 512;
  MockServiceWorkerDataPipeStateNotifier notifier;
  mojo::ScopedDataPipeConsumerHandle data_consumer;
  base::RunLoop loop;
  reader->ReadData(
      kBigEnough, notifier.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](mojo::ScopedDataPipeConsumerHandle pipe) {
        data_consumer = std::move(pipe);
        loop.Quit();
      }));
  loop.Run();

  std::string body = ReadDataPipe(std::move(data_consumer));
  int rv = notifier.WaitUntilComplete();

  EXPECT_EQ(static_cast<int>(kExpectedHttpBody.size()), rv);
  if (rv <= 0)
    return false;

  bool status_match =
      std::string("HONKYDORY") == out.response_head->headers->GetStatusText();
  bool data_match = kExpectedHttpBody == body;

  EXPECT_EQ(out.response_head->headers->GetStatusText(), "HONKYDORY");
  EXPECT_EQ(body, kExpectedHttpBody);
  return status_match && data_match;
}

int WriteResponseMetadata(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    const std::string& metadata) {
  mojo_base::BigBuffer buffer(
      base::as_bytes(base::make_span(metadata.data(), metadata.length())));

  mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter>
      metadata_writer;
  storage->CreateResourceMetadataWriter(
      id, metadata_writer.BindNewPipeAndPassReceiver());
  int rv = 0;
  base::RunLoop loop;
  metadata_writer->WriteMetadata(std::move(buffer),
                                 base::BindLambdaForTesting([&](int result) {
                                   rv = result;
                                   loop.Quit();
                                 }));
  loop.Run();
  return rv;
}

int WriteMetadata(ServiceWorkerVersion* version,
                  const GURL& url,
                  const std::string& metadata) {
  const std::vector<uint8_t> data(metadata.begin(), metadata.end());
  EXPECT_TRUE(version);
  TestCompletionCallback cb;
  version->script_cache_map()->WriteMetadata(url, data, cb.callback());
  return cb.WaitForResult();
}

int ClearMetadata(ServiceWorkerVersion* version, const GURL& url) {
  EXPECT_TRUE(version);
  TestCompletionCallback cb;
  version->script_cache_map()->ClearMetadata(url, cb.callback());
  return cb.WaitForResult();
}

bool VerifyResponseMetadata(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    const std::string& expected_metadata) {
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
  storage->CreateResourceReader(id, reader.BindNewPipeAndPassReceiver());
  ReadResponseHeadResult out = ReadResponseHead(storage, id);
  if (!out.metadata.has_value())
    return false;
  EXPECT_EQ(0, memcmp(expected_metadata.data(), out.metadata->data(),
                      expected_metadata.length()));
  return true;
}

class ServiceWorkerStorageTest : public testing::Test {
 public:
  ServiceWorkerStorageTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override { InitializeTestHelper(); }

  void TearDown() override {
    helper_.reset();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  bool InitUserDataDirectory() {
    if (!user_data_directory_.CreateUniqueTempDir())
      return false;
    user_data_directory_path_ = user_data_directory_.GetPath();
    return true;
  }

  void InitializeTestHelper() {
    helper_.reset(new EmbeddedWorkerTestHelper(user_data_directory_path_));
    // TODO(falken): Figure out why RunUntilIdle is needed.
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerRegistry* registry() { return context()->registry(); }
  ServiceWorkerStorage* storage() { return registry()->storage(); }
  mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage_control() {
    return registry()->GetRemoteStorageControl();
  }
  ServiceWorkerDatabase* database() { return storage()->database_.get(); }

 protected:
  const std::set<url::Origin>& registered_origins() {
    return storage()->registered_origins_;
  }

  void LazyInitialize() { storage()->LazyInitializeForTest(); }

  blink::ServiceWorkerStatusCode StoreRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->StoreRegistration(
        registration.get(), version.get(),
        base::BindOnce(&StatusCallback, loop.QuitClosure(), &result));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode DeleteRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      const GURL& origin) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->DeleteRegistration(
        registration, origin,
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  ServiceWorkerDatabase::Status DeleteRegistrationById(int64_t registration_id,
                                                       const GURL& origin) {
    ServiceWorkerDatabase::Status result;
    base::RunLoop loop;
    storage()->DeleteRegistration(
        registration_id, origin,
        base::BindLambdaForTesting(
            [&](ServiceWorkerDatabase::Status status,
                ServiceWorkerStorage::OriginState, int64_t deleted_version,
                const std::vector<int64_t>& newly_purgeable_resources) {
              result = status;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode GetAllRegistrationsInfos(
      std::vector<ServiceWorkerRegistrationInfo>* registrations) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->GetAllRegistrationsInfos(base::BindLambdaForTesting(
        [&](blink::ServiceWorkerStatusCode status,
            const std::vector<ServiceWorkerRegistrationInfo>& infos) {
          result = status;
          *registrations = infos;
          loop.Quit();
        }));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode GetStorageUsageForOrigin(
      const url::Origin& origin,
      int64_t& out_usage) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->GetStorageUsageForOrigin(
        origin, base::BindLambdaForTesting(
                    [&](blink::ServiceWorkerStatusCode status, int64_t usage) {
                      result = status;
                      out_usage = usage;
                      loop.Quit();
                    }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode GetRegistrationsForOrigin(
      const url::Origin& origin,
      std::vector<scoped_refptr<ServiceWorkerRegistration>>* registrations) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->GetRegistrationsForOrigin(
        origin,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
                    found_registrations) {
              result = status;
              *registrations = found_registrations;
              loop.Quit();
            }));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode GetUserData(
      int64_t registration_id,
      const std::vector<std::string>& keys,
      std::vector<std::string>* data) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->GetUserData(
        registration_id, keys,
        base::BindOnce(&UserDataCallback, loop.QuitClosure(), data, &result));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode GetUserDataByKeyPrefix(
      int64_t registration_id,
      const std::string& key_prefix,
      std::vector<std::string>* data) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->GetUserDataByKeyPrefix(
        registration_id, key_prefix,
        base::BindOnce(&UserDataCallback, loop.QuitClosure(), data, &result));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode StoreUserData(
      int64_t registration_id,
      const url::Origin& origin,
      const std::vector<std::pair<std::string, std::string>>& key_value_pairs) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->StoreUserData(
        registration_id, origin, key_value_pairs,
        base::BindOnce(&StatusCallback, loop.QuitClosure(), &result));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode ClearUserData(
      int64_t registration_id,
      const std::vector<std::string>& keys) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->ClearUserData(
        registration_id, keys,
        base::BindOnce(&StatusCallback, loop.QuitClosure(), &result));
    EXPECT_FALSE(result);  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode ClearUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& key_prefixes) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->ClearUserDataByKeyPrefixes(
        registration_id, key_prefixes,
        base::BindOnce(&StatusCallback, loop.QuitClosure(), &result));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode GetUserDataForAllRegistrations(
      const std::string& key,
      std::vector<std::pair<int64_t, std::string>>* data) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->GetUserDataForAllRegistrations(
        key,
        base::BindLambdaForTesting(
            [&](const std::vector<std::pair<int64_t, std::string>>& user_data,
                blink::ServiceWorkerStatusCode status) {
              result = status;
              *data = user_data;
              loop.Quit();
            }));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode ClearUserDataForAllRegistrationsByKeyPrefix(
      const std::string& key_prefix) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->ClearUserDataForAllRegistrationsByKeyPrefix(
        key_prefix,
        base::BindOnce(&StatusCallback, loop.QuitClosure(), &result));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode UpdateToActiveState(
      scoped_refptr<ServiceWorkerRegistration> registration) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->UpdateToActiveState(
        registration->id(), registration->scope().GetOrigin(),
        base::BindOnce(&StatusCallback, loop.QuitClosure(), &result));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode UpdateLastUpdateCheckTime(
      scoped_refptr<ServiceWorkerRegistration> registration) {
    base::RunLoop loop;
    base::Optional<blink::ServiceWorkerStatusCode> result;
    registry()->UpdateLastUpdateCheckTime(
        registration->id(), registration->scope().GetOrigin(),
        registration->last_update_check(),
        base::BindOnce(&StatusCallback, loop.QuitClosure(), &result));
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode FindRegistrationForClientUrl(
      const GURL& document_url,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->FindRegistrationForClientUrl(
        document_url, base::BindOnce(&FindCallback, loop.QuitClosure(), &result,
                                     registration));
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode FindRegistrationForScope(
      const GURL& scope,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->FindRegistrationForScope(
        scope, base::BindOnce(&FindCallback, loop.QuitClosure(), &result,
                              registration));
    EXPECT_FALSE(result);  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode FindRegistrationForId(
      int64_t registration_id,
      const url::Origin& origin,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->FindRegistrationForId(
        registration_id, origin,
        base::BindOnce(&FindCallback, loop.QuitClosure(), &result,
                       registration));
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode FindRegistrationForIdOnly(
      int64_t registration_id,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    base::Optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->FindRegistrationForIdOnly(
        registration_id, base::BindOnce(&FindCallback, loop.QuitClosure(),
                                        &result, registration));
    loop.Run();
    return result.value();
  }

  int64_t GetNewRegistrationId() {
    int64_t result;
    base::RunLoop loop;
    storage()->GetNewRegistrationId(base::BindLambdaForTesting([&](int64_t id) {
      result = id;
      loop.Quit();
    }));
    loop.Run();
    return result;
  }

  int64_t GetNewVersionId() {
    int64_t result;
    base::RunLoop loop;
    storage()->GetNewVersionId(base::BindLambdaForTesting([&](int64_t id) {
      result = id;
      loop.Quit();
    }));
    loop.Run();
    return result;
  }

  int64_t GetNewResourceId() {
    int64_t result;
    base::RunLoop loop;
    storage()->GetNewResourceId(base::BindLambdaForTesting([&](int64_t id) {
      result = id;
      loop.Quit();
    }));
    loop.Run();
    return result;
  }

  base::circular_deque<int64_t> GetPurgingResources() {
    return storage()->purgeable_resource_ids_;
  }

  // Directly writes a registration using
  // ServiceWorkerDatabase::WriteRegistration rather than
  // ServiceWorkerStorage::StoreRegistration. Useful for simulating a
  // registration written by an earlier version of Chrome.
  void WriteRegistrationToDB(const RegistrationData& registration,
                             const std::vector<ResourceRecord>& resources) {
    ServiceWorkerDatabase* database_raw = database();
    base::RunLoop loop;
    storage()->database_task_runner_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          ServiceWorkerDatabase::DeletedVersion deleted_version;
          ASSERT_EQ(ServiceWorkerDatabase::Status::kOk,
                    database_raw->WriteRegistration(registration, resources,
                                                    &deleted_version));
          loop.Quit();
        }));
    loop.Run();
  }

  std::vector<int64_t> GetPurgeableResourceIdsFromDB() {
    std::vector<int64_t> ids;
    base::RunLoop loop;
    ServiceWorkerDatabase* database_raw = database();
    storage()->database_task_runner_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
                    database_raw->GetPurgeableResourceIds(&ids));
          loop.Quit();
        }));
    loop.Run();
    return ids;
  }

  std::vector<int64_t> GetUncommittedResourceIdsFromDB() {
    std::vector<int64_t> ids;
    base::RunLoop loop;
    ServiceWorkerDatabase* database_raw = database();
    storage()->database_task_runner_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
                    database_raw->GetUncommittedResourceIds(&ids));
          loop.Quit();
        }));
    loop.Run();
    return ids;
  }

  // user_data_directory_ must be declared first to preserve destructor order.
  base::ScopedTempDir user_data_directory_;
  base::FilePath user_data_directory_path_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  BrowserTaskEnvironment task_environment_;
};

TEST_F(ServiceWorkerStorageTest, DisabledStorage) {
  const GURL kScope("http://www.example.com/scope/");
  const url::Origin kOrigin = url::Origin::Create(kScope);
  const GURL kScript("http://www.example.com/script.js");
  const GURL kDocumentUrl("http://www.example.com/scope/document.html");
  const int64_t kRegistrationId = 0;
  const int64_t kVersionId = 0;
  const int64_t kResourceId = 0;

  registry()->DisableDeleteAndStartOverForTesting();
  LazyInitialize();
  storage()->Disable();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            FindRegistrationForScope(kScope, &found_registration));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            FindRegistrationForId(kRegistrationId, url::Origin::Create(kScope),
                                  &found_registration));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  EXPECT_FALSE(registry()->GetUninstallingRegistration(kScope.GetOrigin()));

  std::vector<scoped_refptr<ServiceWorkerRegistration>> found_registrations;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            GetRegistrationsForOrigin(url::Origin::Create(kScope),
                                      &found_registrations));

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            GetAllRegistrationsInfos(&all_registrations));

  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(options, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kScript, blink::mojom::ScriptType::kClassic,
      kVersionId,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            StoreRegistration(live_registration, live_version));

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            UpdateToActiveState(live_registration));

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            DeleteRegistration(live_registration, kScope.GetOrigin()));

  // Response reader and writer created by the disabled storage should fail to
  // access the disk cache.
  ReadResponseHeadResult out = ReadResponseHead(storage_control(), kResourceId);
  EXPECT_EQ(net::ERR_CACHE_MISS, out.result);
  EXPECT_EQ(net::ERR_FAILED,
            WriteBasicResponse(storage_control(), kResourceId));
  EXPECT_EQ(net::ERR_FAILED,
            WriteResponseMetadata(storage_control(), kResourceId, "foo"));

  const std::string kUserDataKey = "key";
  std::vector<std::string> user_data_out;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            GetUserData(kRegistrationId, {kUserDataKey}, &user_data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            GetUserDataByKeyPrefix(kRegistrationId, "prefix", &user_data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            StoreUserData(kRegistrationId, kOrigin, {{kUserDataKey, "foo"}}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            ClearUserData(kRegistrationId, {kUserDataKey}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            ClearUserDataByKeyPrefixes(kRegistrationId, {"prefix"}));
  std::vector<std::pair<int64_t, std::string>> data_list_out;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            GetUserDataForAllRegistrations(kUserDataKey, &data_list_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            ClearUserDataForAllRegistrationsByKeyPrefix("prefix"));

  // Next available ids should be invalid.
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerRegistrationId,
            GetNewRegistrationId());
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerVersionId, GetNewVersionId());
  EXPECT_EQ(blink::mojom::kInvalidServiceWorkerResourceId, GetNewResourceId());
}

TEST_F(ServiceWorkerStorageTest, StoreFindUpdateDeleteRegistration) {
  const GURL kScope("http://www.test.not/scope/");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const GURL kResource1("http://www.test.not/scope/resource1.js");
  const int64_t kResource1Size = 1591234;
  const GURL kResource2("http://www.test.not/scope/resource2.js");
  const int64_t kResource2Size = 51;
  const int64_t kRegistrationId = 0;
  const int64_t kVersionId = 0;
  const base::Time kToday = base::Time::Now();
  const base::Time kYesterday = kToday - base::TimeDelta::FromDays(1);
  std::set<blink::mojom::WebFeature> used_features = {
      blink::mojom::WebFeature::kServiceWorkerControlledPage,
      blink::mojom::WebFeature::kReferrerPolicyHeader,
      blink::mojom::WebFeature::kLocationOrigin};

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // We shouldn't find anything without having stored anything.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForScope(kScope, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForId(kRegistrationId, url::Origin::Create(kScope),
                                  &found_registration));
  EXPECT_FALSE(found_registration.get());

  std::vector<ResourceRecord> resources;
  resources.push_back(CreateResourceRecord(1, kResource1, kResource1Size));
  resources.push_back(CreateResourceRecord(2, kResource2, kResource2Size));

  // Store something.
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(options, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kResource1, blink::mojom::ScriptType::kClassic,
      kVersionId,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());
  live_version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  live_version->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_version->script_cache_map()->SetResources(resources);
  live_version->set_used_features(
      std::set<blink::mojom::WebFeature>(used_features));
  network::CrossOriginEmbedderPolicy coep_require_corp;
  coep_require_corp.value =
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  live_version->set_cross_origin_embedder_policy(coep_require_corp);
  live_registration->SetWaitingVersion(live_version);
  live_registration->set_last_update_check(kYesterday);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration, live_version));

  // Now we should find it and get the live ptr back immediately.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  EXPECT_EQ(kResource1Size + kResource2Size,
            live_registration->resources_total_size_bytes());
  EXPECT_EQ(kResource1Size + kResource2Size,
            found_registration->resources_total_size_bytes());
  EXPECT_EQ(used_features,
            found_registration->waiting_version()->used_features());
  EXPECT_EQ(
      found_registration->waiting_version()->cross_origin_embedder_policy(),
      coep_require_corp);
  found_registration = nullptr;

  // But FindRegistrationForScope is always async.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForScope(kScope, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  // Can be found by id too.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForId(kRegistrationId, url::Origin::Create(kScope),
                                  &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  // Can be found by just the id too.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  // Drop the live registration, but keep the version live.
  live_registration = nullptr;

  // Now FindRegistrationForClientUrl should be async.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());

  // Check that sizes are populated correctly
  EXPECT_EQ(live_version.get(), found_registration->waiting_version());
  EXPECT_EQ(kResource1Size + kResource2Size,
            found_registration->resources_total_size_bytes());
  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetAllRegistrationsInfos(&all_registrations));
  EXPECT_EQ(1u, all_registrations.size());
  ServiceWorkerRegistrationInfo info = all_registrations[0];
  EXPECT_EQ(kResource1Size + kResource2Size, info.stored_version_size_bytes);
  all_registrations.clear();

  // Finding by origin should provide the same result if origin is kScope.
  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      registrations_for_origin;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForOrigin(url::Origin::Create(kScope),
                                      &registrations_for_origin));
  EXPECT_EQ(1u, registrations_for_origin.size());
  registrations_for_origin.clear();

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForOrigin(
                url::Origin::Create(GURL("http://example.com/")),
                &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  found_registration = nullptr;

  // Drop the live version too.
  live_version = nullptr;

  // And FindRegistrationForScope is always async.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForScope(kScope, &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->active_version());
  ASSERT_TRUE(found_registration->waiting_version());
  EXPECT_EQ(kYesterday, found_registration->last_update_check());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            found_registration->waiting_version()->status());

  // Update to active and update the last check time.
  scoped_refptr<ServiceWorkerVersion> temp_version =
      found_registration->waiting_version();
  temp_version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  found_registration->SetActiveVersion(temp_version);
  temp_version = nullptr;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            UpdateToActiveState(found_registration));
  found_registration->set_last_update_check(kToday);
  UpdateLastUpdateCheckTime(found_registration.get());

  found_registration = nullptr;

  // Trying to update a unstored registration to active should fail.
  scoped_refptr<ServiceWorkerRegistration> unstored_registration =
      new ServiceWorkerRegistration(options, kRegistrationId + 1,
                                    context()->AsWeakPtr());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            UpdateToActiveState(unstored_registration));
  unstored_registration = nullptr;

  // The Find methods should return a registration with an active version
  // and the expected update time.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->waiting_version());
  ASSERT_TRUE(found_registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            found_registration->active_version()->status());
  EXPECT_EQ(kToday, found_registration->last_update_check());
}

TEST_F(ServiceWorkerStorageTest, InstallingRegistrationsAreFindable) {
  const GURL kScope("http://www.test.not/scope/");
  const GURL kScript("http://www.test.not/script.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const int64_t kVersionId = 0;

  LazyInitialize();

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Create an unstored registration.
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      CreateNewServiceWorkerRegistration(registry(), options);
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kScript, blink::mojom::ScriptType::kClassic,
      kVersionId,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());
  live_version->SetStatus(ServiceWorkerVersion::INSTALLING);
  live_registration->SetWaitingVersion(live_version);

  const int64_t kRegistrationId = live_registration->id();

  // Should not be findable, including by GetAllRegistrationsInfos.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForId(kRegistrationId, url::Origin::Create(kScope),
                                  &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForScope(kScope, &found_registration));
  EXPECT_FALSE(found_registration.get());

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetAllRegistrationsInfos(&all_registrations));
  EXPECT_TRUE(all_registrations.empty());

  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      registrations_for_origin;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForOrigin(url::Origin::Create(kScope),
                                      &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForOrigin(
                url::Origin::Create(GURL("http://example.com/")),
                &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  // Notify storage of it being installed.
  registry()->NotifyInstallingRegistration(live_registration.get());

  // Now should be findable.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForId(kRegistrationId, url::Origin::Create(kScope),
                                  &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForScope(kScope, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetAllRegistrationsInfos(&all_registrations));
  EXPECT_EQ(1u, all_registrations.size());
  all_registrations.clear();

  // Finding by origin should provide the same result if origin is kScope.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForOrigin(url::Origin::Create(kScope),
                                      &registrations_for_origin));
  EXPECT_EQ(1u, registrations_for_origin.size());
  registrations_for_origin.clear();

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForOrigin(
                url::Origin::Create(GURL("http://example.com/")),
                &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  // Notify storage of installation no longer happening.
  registry()->NotifyDoneInstallingRegistration(
      live_registration.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);

  // Once again, should not be findable.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForId(kRegistrationId, url::Origin::Create(kScope),
                                  &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForIdOnly(kRegistrationId, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForClientUrl(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForScope(kScope, &found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetAllRegistrationsInfos(&all_registrations));
  EXPECT_TRUE(all_registrations.empty());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForOrigin(url::Origin::Create(kScope),
                                      &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForOrigin(
                url::Origin::Create(GURL("http://example.com/")),
                &registrations_for_origin));
  EXPECT_TRUE(registrations_for_origin.empty());
}

TEST_F(ServiceWorkerStorageTest, StoreUserData) {
  const GURL kScope("http://www.test.not/scope/");
  const url::Origin kOrigin = url::Origin::Create(kScope);
  const GURL kScript("http://www.test.not/script.js");
  LazyInitialize();

  // Store a registration.
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                /*resource_id=*/1);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration,
                              live_registration->waiting_version()));
  const int64_t kRegistrationId = live_registration->id();

  // Store user data associated with the registration.
  std::vector<std::string> data_out;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreUserData(kRegistrationId, kOrigin, {{"key", "data"}}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  ASSERT_EQ(1u, data_out.size());
  EXPECT_EQ("data", data_out[0]);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"unknown_key"}, &data_out));
  std::vector<std::pair<int64_t, std::string>> data_list_out;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserDataForAllRegistrations("key", &data_list_out));
  ASSERT_EQ(1u, data_list_out.size());
  EXPECT_EQ(kRegistrationId, data_list_out[0].first);
  EXPECT_EQ("data", data_list_out[0].second);
  data_list_out.clear();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserDataForAllRegistrations("unknown_key", &data_list_out));
  EXPECT_EQ(0u, data_list_out.size());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            ClearUserData(kRegistrationId, {"key"}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"key"}, &data_out));

  // Write/overwrite multiple user data keys.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreUserData(
                kRegistrationId, kOrigin,
                {{"key", "overwrite"}, {"key3", "data3"}, {"key4", "data4"}}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"key2"}, &data_out));
  EXPECT_TRUE(data_out.empty());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserData(kRegistrationId, {"key", "key3", "key4"}, &data_out));
  ASSERT_EQ(3u, data_out.size());
  EXPECT_EQ("overwrite", data_out[0]);
  EXPECT_EQ("data3", data_out[1]);
  EXPECT_EQ("data4", data_out[2]);
  // Multiple gets fail if one is not found.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"key", "key2"}, &data_out));
  EXPECT_TRUE(data_out.empty());

  // Delete multiple user data keys, even if some are not found.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            ClearUserData(kRegistrationId, {"key", "key2", "key3"}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"key2"}, &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"key3"}, &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserData(kRegistrationId, {"key4"}, &data_out));
  ASSERT_EQ(1u, data_out.size());
  EXPECT_EQ("data4", data_out[0]);

  // Get/delete multiple user data keys by prefixes.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreUserData(kRegistrationId, kOrigin,
                          {{"prefixA", "data1"},
                           {"prefixA2", "data2"},
                           {"prefixB", "data3"},
                           {"prefixC", "data4"}}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserDataByKeyPrefix(kRegistrationId, "prefix", &data_out));
  ASSERT_EQ(4u, data_out.size());
  EXPECT_EQ("data1", data_out[0]);
  EXPECT_EQ("data2", data_out[1]);
  EXPECT_EQ("data3", data_out[2]);
  EXPECT_EQ("data4", data_out[3]);
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      ClearUserDataByKeyPrefixes(kRegistrationId, {"prefixA", "prefixC"}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserDataByKeyPrefix(kRegistrationId, "prefix", &data_out));
  ASSERT_EQ(1u, data_out.size());
  EXPECT_EQ("data3", data_out[0]);

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            ClearUserDataForAllRegistrationsByKeyPrefix("prefixB"));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserDataByKeyPrefix(kRegistrationId, "prefix", &data_out));
  EXPECT_TRUE(data_out.empty());

  // User data should be deleted when the associated registration is deleted.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreUserData(kRegistrationId, kOrigin, {{"key", "data"}}));
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  ASSERT_EQ(1u, data_out.size());
  ASSERT_EQ("data", data_out[0]);

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(live_registration, kScope.GetOrigin()));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  data_list_out.clear();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserDataForAllRegistrations("key", &data_list_out));
  EXPECT_EQ(0u, data_list_out.size());

  // Data access with an invalid registration id should be failed.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            StoreUserData(blink::mojom::kInvalidServiceWorkerRegistrationId,
                          kOrigin, {{"key", "data"}}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            GetUserData(blink::mojom::kInvalidServiceWorkerRegistrationId,
                        {"key"}, &data_out));
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kErrorFailed,
      GetUserDataByKeyPrefix(blink::mojom::kInvalidServiceWorkerRegistrationId,
                             "prefix", &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            ClearUserData(blink::mojom::kInvalidServiceWorkerRegistrationId,
                          {"key"}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            ClearUserDataByKeyPrefixes(
                blink::mojom::kInvalidServiceWorkerRegistrationId, {"prefix"}));

  // Data access with an empty key should be failed.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            StoreUserData(kRegistrationId, kOrigin,
                          std::vector<std::pair<std::string, std::string>>()));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            StoreUserData(kRegistrationId, kOrigin, {{std::string(), "data"}}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            StoreUserData(kRegistrationId, kOrigin,
                          {{std::string(), "data"}, {"key", "data"}}));
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kErrorFailed,
      GetUserData(kRegistrationId, std::vector<std::string>(), &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            GetUserDataByKeyPrefix(kRegistrationId, std::string(), &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            GetUserData(kRegistrationId, {std::string()}, &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            GetUserData(kRegistrationId, {std::string(), "key"}, &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            ClearUserData(kRegistrationId, std::vector<std::string>()));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            ClearUserData(kRegistrationId, {std::string()}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            ClearUserData(kRegistrationId, {std::string(), "key"}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            ClearUserDataByKeyPrefixes(kRegistrationId, {}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            ClearUserDataByKeyPrefixes(kRegistrationId, {std::string()}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            ClearUserDataForAllRegistrationsByKeyPrefix(std::string()));
  data_list_out.clear();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed,
            GetUserDataForAllRegistrations(std::string(), &data_list_out));
}

// The *_BeforeInitialize tests exercise the API before LazyInitialize() is
// called.
TEST_F(ServiceWorkerStorageTest, StoreUserData_BeforeInitialize) {
  const int kRegistrationId = 0;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            StoreUserData(kRegistrationId,
                          url::Origin::Create(GURL("https://example.com")),
                          {{"key", "data"}}));
}

TEST_F(ServiceWorkerStorageTest, GetUserData_BeforeInitialize) {
  const int kRegistrationId = 0;
  std::vector<std::string> data_out;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserData(kRegistrationId, {"key"}, &data_out));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            GetUserDataByKeyPrefix(kRegistrationId, "prefix", &data_out));
}

TEST_F(ServiceWorkerStorageTest, ClearUserData_BeforeInitialize) {
  const int kRegistrationId = 0;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            ClearUserData(kRegistrationId, {"key"}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            ClearUserDataByKeyPrefixes(kRegistrationId, {"prefix"}));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            ClearUserDataForAllRegistrationsByKeyPrefix("key"));
}

TEST_F(ServiceWorkerStorageTest,
       GetUserDataForAllRegistrations_BeforeInitialize) {
  std::vector<std::pair<int64_t, std::string>> data_list_out;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetUserDataForAllRegistrations("key", &data_list_out));
  EXPECT_TRUE(data_list_out.empty());
}

class ServiceWorkerResourceStorageTest : public ServiceWorkerStorageTest {
 public:
  void SetUp() override {
    ServiceWorkerStorageTest::SetUp();
    LazyInitialize();

    scope_ = GURL("http://www.test.not/scope/");
    script_ = GURL("http://www.test.not/script.js");
    import_ = GURL("http://www.test.not/import.js");
    document_url_ = GURL("http://www.test.not/scope/document.html");
    resource_id1_ = GetNewResourceIdSync(storage_control());
    resource_id2_ = GetNewResourceIdSync(storage_control());
    resource_id1_size_ = 239193;
    resource_id2_size_ = 59923;

    // Cons up a new registration+version with two script resources.
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ = CreateNewServiceWorkerRegistration(registry(), options);
    scoped_refptr<ServiceWorkerVersion> version = CreateNewServiceWorkerVersion(
        registry(), registration_.get(), script_, options.type);
    version->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST);
    version->SetStatus(ServiceWorkerVersion::INSTALLED);

    std::vector<ResourceRecord> resources;
    resources.push_back(
        CreateResourceRecord(resource_id1_, script_, resource_id1_size_));
    resources.push_back(
        CreateResourceRecord(resource_id2_, import_, resource_id2_size_));
    version->script_cache_map()->SetResources(resources);

    registration_->SetWaitingVersion(version);

    registration_id_ = registration_->id();
    version_id_ = version->version_id();

    // Add the resources ids to the uncommitted list.
    registry()->StoreUncommittedResourceId(resource_id1_, scope_);
    registry()->StoreUncommittedResourceId(resource_id2_, scope_);
    // Make sure that StoreUncommittedResourceId mojo message is received.
    storage_control().FlushForTesting();

    std::vector<int64_t> verify_ids = GetUncommittedResourceIdsFromDB();
    EXPECT_EQ(2u, verify_ids.size());

    // And dump something in the disk cache for them.
    WriteBasicResponse(storage_control(), resource_id1_);
    WriteBasicResponse(storage_control(), resource_id2_);
    EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
    EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, true));

    // Storing the registration/version should take the resources ids out
    // of the uncommitted list.
    EXPECT_EQ(
        blink::ServiceWorkerStatusCode::kOk,
        StoreRegistration(registration_, registration_->waiting_version()));
    verify_ids = GetUncommittedResourceIdsFromDB();
    EXPECT_TRUE(verify_ids.empty());
  }

 protected:
  GURL scope_;
  GURL script_;
  GURL import_;
  GURL document_url_;
  int64_t registration_id_;
  int64_t version_id_;
  int64_t resource_id1_;
  uint64_t resource_id1_size_;
  int64_t resource_id2_;
  uint64_t resource_id2_size_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
};

class ServiceWorkerResourceStorageDiskTest
    : public ServiceWorkerResourceStorageTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(InitUserDataDirectory());
    ServiceWorkerResourceStorageTest::SetUp();
  }
};

TEST_F(ServiceWorkerResourceStorageTest,
       WriteMetadataWithServiceWorkerResponseMetadataWriter) {
  const char kMetadata1[] = "Test metadata";
  const char kMetadata2[] = "small";
  int64_t new_resource_id_ = GetNewResourceIdSync(storage_control());
  // Writing metadata to nonexistent resoirce ID must fail.
  EXPECT_GE(0, WriteResponseMetadata(storage_control(), new_resource_id_,
                                     kMetadata1));

  // Check metadata is written.
  EXPECT_EQ(
      static_cast<int>(strlen(kMetadata1)),
      WriteResponseMetadata(storage_control(), resource_id1_, kMetadata1));
  EXPECT_TRUE(
      VerifyResponseMetadata(storage_control(), resource_id1_, kMetadata1));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));

  // Check metadata is written and truncated.
  EXPECT_EQ(
      static_cast<int>(strlen(kMetadata2)),
      WriteResponseMetadata(storage_control(), resource_id1_, kMetadata2));
  EXPECT_TRUE(
      VerifyResponseMetadata(storage_control(), resource_id1_, kMetadata2));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));

  // Check metadata is deleted.
  EXPECT_EQ(0, WriteResponseMetadata(storage_control(), resource_id1_, ""));
  EXPECT_FALSE(VerifyResponseMetadata(storage_control(), resource_id1_, ""));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
}

TEST_F(ServiceWorkerResourceStorageTest,
       WriteMetadataWithServiceWorkerScriptCacheMap) {
  const char kMetadata1[] = "Test metadata";
  const char kMetadata2[] = "small";
  ServiceWorkerVersion* version = registration_->waiting_version();
  EXPECT_TRUE(version);

  // Writing metadata to nonexistent URL must fail.
  EXPECT_GE(0,
            WriteMetadata(version, GURL("http://www.test.not/nonexistent.js"),
                          kMetadata1));
  // Clearing metadata of nonexistent URL must fail.
  EXPECT_GE(0,
            ClearMetadata(version, GURL("http://www.test.not/nonexistent.js")));

  // Check metadata is written.
  EXPECT_EQ(static_cast<int>(strlen(kMetadata1)),
            WriteMetadata(version, script_, kMetadata1));
  EXPECT_TRUE(
      VerifyResponseMetadata(storage_control(), resource_id1_, kMetadata1));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));

  // Check metadata is written and truncated.
  EXPECT_EQ(static_cast<int>(strlen(kMetadata2)),
            WriteMetadata(version, script_, kMetadata2));
  EXPECT_TRUE(
      VerifyResponseMetadata(storage_control(), resource_id1_, kMetadata2));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));

  // Check metadata is deleted.
  EXPECT_EQ(0, ClearMetadata(version, script_));
  EXPECT_FALSE(VerifyResponseMetadata(storage_control(), resource_id1_, ""));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
}

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_NoLiveVersion) {
  // Deleting the registration should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from that
  // list.
  base::RunLoop loop;
  storage()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(registration_, scope_.GetOrigin()));
  // At this point registration_->waiting_version() has a remote reference, so
  // the resources should be in the purgeable list.
  EXPECT_EQ(2u, GetPurgeableResourceIdsFromDB().size());

  registration_->SetWaitingVersion(nullptr);
  loop.Run();

  // registration_->waiting_version() is cleared. The resources should be
  // purged at this point.
  EXPECT_TRUE(GetPurgeableResourceIdsFromDB().empty());
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_WaitingVersion) {
  // Deleting the registration should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from that
  // list.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(registration_, scope_.GetOrigin()));
  EXPECT_EQ(2u, GetPurgeableResourceIdsFromDB().size());

  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, false));

  // Doom the version. The resources should be purged.
  base::RunLoop loop;
  storage()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  registration_->waiting_version()->Doom();
  loop.Run();
  EXPECT_TRUE(GetPurgeableResourceIdsFromDB().empty());

  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_ActiveVersion) {
  // Promote the worker to active and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registration_->active_version()->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registry()->UpdateToActiveState(registration_->id(),
                                  registration_->scope().GetOrigin(),
                                  base::DoNothing());
  ServiceWorkerRemoteContainerEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      CreateContainerHostForWindow(33 /* dummy render process id */,
                                   true /* is_parent_frame_secure */,
                                   context()->AsWeakPtr(), &remote_endpoint);
  registration_->active_version()->AddControllee(container_host.get());

  // Deleting the registration should move the resources to the purgeable list
  // but keep them available.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(registration_, scope_.GetOrigin()));
  EXPECT_EQ(2u, GetPurgeableResourceIdsFromDB().size());

  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, true));

  // Dooming the version should cause the resources to be deleted.
  base::RunLoop loop;
  storage()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  registration_->active_version()->RemoveControllee(
      container_host->client_uuid());
  registration_->active_version()->Doom();
  loop.Run();
  EXPECT_TRUE(GetPurgeableResourceIdsFromDB().empty());

  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageDiskTest, CleanupOnRestart) {
  // Promote the worker to active and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registration_->active_version()->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetWaitingVersion(nullptr);
  registry()->UpdateToActiveState(registration_->id(),
                                  registration_->scope().GetOrigin(),
                                  base::DoNothing());
  ServiceWorkerRemoteContainerEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      CreateContainerHostForWindow(33 /* dummy render process id */,
                                   true /* is_parent_frame_secure */,
                                   context()->AsWeakPtr(), &remote_endpoint);
  registration_->active_version()->AddControllee(container_host.get());

  // Deleting the registration should move the resources to the purgeable list
  // but keep them available.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(registration_, scope_.GetOrigin()));
  std::vector<int64_t> verify_ids = GetPurgeableResourceIdsFromDB();
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, true));

  // Also add an uncommitted resource.
  int64_t kStaleUncommittedResourceId = GetNewResourceIdSync(storage_control());
  registry()->StoreUncommittedResourceId(kStaleUncommittedResourceId,
                                         registration_->scope());
  // Make sure that StoreUncommittedResourceId mojo message is received.
  storage_control().FlushForTesting();
  verify_ids = GetUncommittedResourceIdsFromDB();
  EXPECT_EQ(1u, verify_ids.size());
  WriteBasicResponse(storage_control(), kStaleUncommittedResourceId);
  EXPECT_TRUE(VerifyBasicResponse(storage_control(),
                                  kStaleUncommittedResourceId, true));

  // Simulate browser shutdown. The purgeable and uncommitted resources are now
  // stale.
  InitializeTestHelper();
  LazyInitialize();

  // Store a new uncommitted resource. This triggers stale resource cleanup.
  base::RunLoop loop;
  storage()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  int64_t kNewResourceId = GetNewResourceIdSync(storage_control());
  WriteBasicResponse(storage_control(), kNewResourceId);
  registry()->StoreUncommittedResourceId(kNewResourceId,
                                         registration_->scope());
  loop.Run();

  // The stale resources should be purged, but the new resource should persist.
  verify_ids = GetUncommittedResourceIdsFromDB();
  ASSERT_EQ(1u, verify_ids.size());
  EXPECT_EQ(kNewResourceId, *verify_ids.begin());

  verify_ids = GetPurgeableResourceIdsFromDB();
  EXPECT_TRUE(verify_ids.empty());
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(),
                                   kStaleUncommittedResourceId, false));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), kNewResourceId, true));
}

TEST_F(ServiceWorkerResourceStorageDiskTest, DeleteAndStartOver) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  base::RunLoop run_loop;
  base::Optional<ServiceWorkerDatabase::Status> status;
  storage()->DeleteAndStartOver(
      base::BindOnce(&DatabaseStatusCallback, run_loop.QuitClosure(), &status));
  run_loop.Run();

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, *status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
}

TEST_F(ServiceWorkerResourceStorageDiskTest,
       DeleteAndStartOver_UnrelatedFileExists) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  // Create an unrelated file in the database directory to make sure such a file
  // does not prevent DeleteAndStartOver.
  base::FilePath file_path;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(storage()->GetDatabasePath(), &file_path));
  ASSERT_TRUE(base::PathExists(file_path));

  base::RunLoop run_loop;
  base::Optional<ServiceWorkerDatabase::Status> status;
  storage()->DeleteAndStartOver(
      base::BindOnce(&DatabaseStatusCallback, run_loop.QuitClosure(), &status));
  run_loop.Run();

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, *status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
}

TEST_F(ServiceWorkerResourceStorageDiskTest,
       DeleteAndStartOver_OpenedFileExists) {
  EXPECT_FALSE(storage()->IsDisabled());
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));

  // Create an unrelated opened file in the database directory to make sure such
  // a file does not prevent DeleteAndStartOver on non-Windows platforms.
  base::FilePath file_path;
  base::ScopedFILE file = base::CreateAndOpenTemporaryStreamInDir(
      storage()->GetDatabasePath(), &file_path);
  ASSERT_TRUE(file);
  ASSERT_TRUE(base::PathExists(file_path));

  base::RunLoop run_loop;
  base::Optional<ServiceWorkerDatabase::Status> status;
  storage()->DeleteAndStartOver(
      base::BindOnce(&DatabaseStatusCallback, run_loop.QuitClosure(), &status));
  run_loop.Run();

#if defined(OS_WIN)
  // On Windows, deleting the directory containing an opened file should fail.
  EXPECT_EQ(ServiceWorkerDatabase::Status::kErrorIOError, *status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_TRUE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_TRUE(base::DirectoryExists(storage()->GetDatabasePath()));
#else
  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk, *status);
  EXPECT_TRUE(storage()->IsDisabled());
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDiskCachePath()));
  EXPECT_FALSE(base::DirectoryExists(storage()->GetDatabasePath()));
#endif
}

TEST_F(ServiceWorkerResourceStorageTest, UpdateRegistration) {
  // Promote the worker to active worker and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registration_->active_version()->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registry()->UpdateToActiveState(registration_->id(),
                                  registration_->scope().GetOrigin(),
                                  base::DoNothing());
  ServiceWorkerRemoteContainerEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      CreateContainerHostForWindow(
          33 /* dummy render process id */, true /* is_parent_frame_secure */,
          helper_->context()->AsWeakPtr(), &remote_endpoint);
  registration_->active_version()->AddControllee(container_host.get());

  // Make an updated registration.
  scoped_refptr<ServiceWorkerVersion> live_version =
      CreateNewServiceWorkerVersion(registry(), registration_.get(), script_,
                                    blink::mojom::ScriptType::kClassic);
  live_version->SetStatus(ServiceWorkerVersion::NEW);
  registration_->SetWaitingVersion(live_version);
  std::vector<ResourceRecord> records;
  records.push_back(CreateResourceRecord(10, live_version->script_url(), 100));
  live_version->script_cache_map()->SetResources(records);
  live_version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);

  // Writing the registration should move the old version's resources to the
  // purgeable list but keep them available.
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      StoreRegistration(registration_.get(), registration_->waiting_version()));
  EXPECT_EQ(2u, GetPurgeableResourceIdsFromDB().size());
  EXPECT_TRUE(GetPurgingResources().empty());

  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, false));

  // Remove the controllee to allow the new version to become active, making the
  // old version redundant.
  base::RunLoop loop;
  storage()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  scoped_refptr<ServiceWorkerVersion> old_version(
      registration_->active_version());
  old_version->RemoveControllee(container_host->client_uuid());
  registration_->ActivateWaitingVersionWhenReady();
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, old_version->status());

  // Its resources should be purged.
  loop.Run();
  EXPECT_TRUE(GetPurgeableResourceIdsFromDB().empty());
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageTest, UpdateRegistration_NoLiveVersion) {
  // Promote the worker to active worker and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registry()->UpdateToActiveState(registration_->id(),
                                  registration_->scope().GetOrigin(),
                                  base::DoNothing());

  // Make an updated registration.
  scoped_refptr<ServiceWorkerVersion> live_version =
      CreateNewServiceWorkerVersion(registry(), registration_.get(), script_,
                                    blink::mojom::ScriptType::kClassic);
  live_version->SetStatus(ServiceWorkerVersion::NEW);
  registration_->SetWaitingVersion(live_version);
  std::vector<ResourceRecord> records;
  records.push_back(CreateResourceRecord(10, live_version->script_url(), 100));
  live_version->script_cache_map()->SetResources(records);
  live_version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);

  // Writing the registration should purge the old version's resources,
  // since it's not live.
  base::RunLoop loop;
  storage()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      StoreRegistration(registration_.get(), registration_->waiting_version()));
  EXPECT_EQ(2u, GetPurgeableResourceIdsFromDB().size());

  // Destroy the active version.
  registration_->UnsetVersion(registration_->active_version());

  // The resources should be purged.
  loop.Run();
  EXPECT_TRUE(GetPurgeableResourceIdsFromDB().empty());
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

// Test fixture that uses disk storage, rather than memory. Useful for tests
// that test persistence by simulating browser shutdown and restart.
class ServiceWorkerStorageDiskTest : public ServiceWorkerStorageTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(InitUserDataDirectory());
    ServiceWorkerStorageTest::SetUp();
  }
};

TEST_F(ServiceWorkerStorageTest, OriginTrialsAbsentEntryAndEmptyEntry) {
  const GURL origin1("http://www1.example.com");
  const GURL scope1("http://www1.example.com/foo/");
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = scope1;
  data1.script = GURL(origin1.spec() + "/script.js");
  data1.version_id = 1000;
  data1.is_active = true;
  data1.resources_total_size_bytes = 100;
  // Don't set origin_trial_tokens to simulate old database entry.
  std::vector<ResourceRecord> resources1;
  resources1.push_back(CreateResourceRecord(1, data1.script, 100));
  WriteRegistrationToDB(data1, resources1);

  const GURL origin2("http://www2.example.com");
  const GURL scope2("http://www2.example.com/foo/");
  RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = scope2;
  data2.script = GURL(origin2.spec() + "/script.js");
  data2.version_id = 2000;
  data2.is_active = true;
  data2.resources_total_size_bytes = 200;
  // Set empty origin_trial_tokens.
  data2.origin_trial_tokens = blink::TrialTokenValidator::FeatureToTokensMap();
  std::vector<ResourceRecord> resources2;
  resources2.push_back(CreateResourceRecord(2, data2.script, 200));
  WriteRegistrationToDB(data2, resources2);

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(scope1, &found_registration));
  ASSERT_TRUE(found_registration->active_version());
  // origin_trial_tokens must be unset.
  EXPECT_FALSE(found_registration->active_version()->origin_trial_tokens());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(scope2, &found_registration));
  ASSERT_TRUE(found_registration->active_version());
  // Empty origin_trial_tokens must exist.
  ASSERT_TRUE(found_registration->active_version()->origin_trial_tokens());
  EXPECT_TRUE(
      found_registration->active_version()->origin_trial_tokens()->empty());
}

// Tests loading a registration that has no navigation preload state.
TEST_F(ServiceWorkerStorageTest, AbsentNavigationPreloadState) {
  const GURL origin1("http://www1.example.com");
  const GURL scope1("http://www1.example.com/foo/");
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = scope1;
  data1.script = GURL(origin1.spec() + "/script.js");
  data1.version_id = 1000;
  data1.is_active = true;
  data1.resources_total_size_bytes = 100;
  // Don't set navigation preload state to simulate old database entry.
  std::vector<ResourceRecord> resources1;
  resources1.push_back(CreateResourceRecord(1, data1.script, 100));
  WriteRegistrationToDB(data1, std::move(resources1));

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(scope1, &found_registration));
  const blink::mojom::NavigationPreloadState& registration_state =
      found_registration->navigation_preload_state();
  EXPECT_FALSE(registration_state.enabled);
  EXPECT_EQ("true", registration_state.header);
  ASSERT_TRUE(found_registration->active_version());
  const blink::mojom::NavigationPreloadState& state =
      found_registration->active_version()->navigation_preload_state();
  EXPECT_FALSE(state.enabled);
  EXPECT_EQ("true", state.header);
}

// Tests storing the script response time for DevTools.
TEST_F(ServiceWorkerStorageDiskTest, ScriptResponseTime) {
  // Make a registration.
  LazyInitialize();
  const GURL kScope("https://example.com/scope");
  const GURL kScript("https://example.com/script.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                /*resource_id=*/1);
  ServiceWorkerVersion* version = registration->waiting_version();

  // Give it a main script response info.
  network::mojom::URLResponseHead response_head;
  response_head.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_head.response_time = base::Time::FromJsTime(19940123);
  version->SetMainScriptResponse(
      std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
          response_head));
  EXPECT_TRUE(version->main_script_response_);
  EXPECT_EQ(response_head.response_time,
            version->script_response_time_for_devtools_);
  EXPECT_EQ(response_head.response_time,
            version->GetInfo().script_response_time);

  // Store the registration.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(registration, version));

  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  InitializeTestHelper();
  LazyInitialize();

  // Read the registration. The main script's response time should be gettable.
  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kScope, &found_registration));
  ASSERT_TRUE(found_registration);
  auto* waiting_version = found_registration->waiting_version();
  ASSERT_TRUE(waiting_version);
  EXPECT_FALSE(waiting_version->main_script_response_);
  EXPECT_EQ(response_head.response_time,
            waiting_version->script_response_time_for_devtools_);
  EXPECT_EQ(response_head.response_time,
            waiting_version->GetInfo().script_response_time);
}

TEST_F(ServiceWorkerStorageDiskTest, RegisteredOriginCount) {
  {
    base::HistogramTester histogram_tester;
    LazyInitialize();
    EXPECT_TRUE(registered_origins().empty());
    histogram_tester.ExpectUniqueSample("ServiceWorker.RegisteredOriginCount",
                                        0, 1);
  }

  std::pair<GURL, GURL> scope_and_script_pairs[] = {
      {GURL("https://www.example.com/scope/"),
       GURL("https://www.example.com/script.js")},
      {GURL("https://www.example.com/scope/foo"),
       GURL("https://www.example.com/script.js")},
      {GURL("https://www.test.com/scope/foobar"),
       GURL("https://www.test.com/script.js")},
      {GURL("https://example.com/scope/"),
       GURL("https://example.com/script.js")},
  };
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  int64_t dummy_resource_id = 1;
  for (const auto& pair : scope_and_script_pairs) {
    registrations.emplace_back(CreateServiceWorkerRegistrationAndVersion(
        context(), pair.first, pair.second, dummy_resource_id));
    ++dummy_resource_id;
  }

  // Store all registrations.
  for (const auto& registration : registrations) {
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
              StoreRegistration(registration, registration->waiting_version()));
  }

  // Simulate browser shutdown and restart.
  registrations.clear();
  InitializeTestHelper();
  {
    base::HistogramTester histogram_tester;
    LazyInitialize();
    EXPECT_EQ(3UL, registered_origins().size());
    histogram_tester.ExpectUniqueSample("ServiceWorker.RegisteredOriginCount",
                                        3, 1);
  }

  // Re-initializing shouldn't re-record the histogram.
  {
    base::HistogramTester histogram_tester;
    LazyInitialize();
    EXPECT_EQ(3UL, registered_origins().size());
    histogram_tester.ExpectTotalCount("ServiceWorker.RegisteredOriginCount", 0);
  }
}

// Tests reading storage usage from database.
TEST_F(ServiceWorkerStorageTest, GetStorageUsageForOrigin) {
  const GURL kScope1("https://www.example.com/foo/");
  const GURL kScript1("https://www.example.com/foo/sw.js");
  const GURL kScope2("https://www.example.com/bar/");
  const GURL kScript2("https://www.example.com/bar/sw.js");
  const GURL kScript3("https://www.example.com/bar/sub.js");

  // Preparation: Store two registrations.
  RegistrationData data1;
  data1.registration_id = 1;
  data1.scope = kScope1;
  data1.script = kScript1;
  data1.version_id = 1;
  data1.is_active = true;
  std::vector<ResourceRecord> resources1;
  resources1.push_back(CreateResourceRecord(1, kScript1, 123));
  data1.resources_total_size_bytes = 0;
  for (auto& resource : resources1) {
    data1.resources_total_size_bytes += resource->size_bytes;
  }
  WriteRegistrationToDB(data1, std::move(resources1));

  RegistrationData data2;
  data2.registration_id = 2;
  data2.scope = kScope2;
  data2.script = kScript2;
  data2.version_id = 1;
  data2.is_active = true;
  std::vector<ResourceRecord> resources2;
  resources2.push_back(CreateResourceRecord(2, kScript2, 456));
  resources2.push_back(CreateResourceRecord(3, kScript3, 789));
  data2.resources_total_size_bytes = 0;
  for (auto& resource : resources2) {
    data2.resources_total_size_bytes += resource->size_bytes;
  }
  WriteRegistrationToDB(data2, std::move(resources2));

  // Storage usage should report total resource size from two registrations.
  const url::Origin origin = url::Origin::Create(kScope1.GetOrigin());
  int64_t usage;
  EXPECT_EQ(GetStorageUsageForOrigin(origin, usage),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(usage, data1.resources_total_size_bytes +
                       data2.resources_total_size_bytes);

  // Delete the first registration. Storage usage should report only the second
  // registration.
  EXPECT_EQ(DeleteRegistrationById(data1.registration_id, origin.GetURL()),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetStorageUsageForOrigin(origin, usage),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(usage, data2.resources_total_size_bytes);

  // Delete the second registration. No storage usage should be reported.
  EXPECT_EQ(DeleteRegistrationById(data2.registration_id, origin.GetURL()),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetStorageUsageForOrigin(origin, usage),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(usage, 0);
}

// Tests loading a registration with a disabled navigation preload
// state.
TEST_F(ServiceWorkerStorageDiskTest, DisabledNavigationPreloadState) {
  LazyInitialize();
  const GURL kScope("https://valid.example.com/scope");
  const GURL kScript("https://valid.example.com/script.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                /*resource_id=*/1);
  ServiceWorkerVersion* version = registration->waiting_version();
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration->SetActiveVersion(version);
  registration->EnableNavigationPreload(false);

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(registration, version));

  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  InitializeTestHelper();
  LazyInitialize();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kScope, &found_registration));
  const blink::mojom::NavigationPreloadState& registration_state =
      found_registration->navigation_preload_state();
  EXPECT_FALSE(registration_state.enabled);
  EXPECT_EQ("true", registration_state.header);
  ASSERT_TRUE(found_registration->active_version());
  const blink::mojom::NavigationPreloadState& state =
      found_registration->active_version()->navigation_preload_state();
  EXPECT_FALSE(state.enabled);
  EXPECT_EQ("true", state.header);
}

}  // namespace service_worker_storage_unittest
}  // namespace content
