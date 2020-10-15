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

storage::mojom::ServiceWorkerRegistrationDataPtr CreateRegistrationData(
    int64_t registration_id,
    int64_t version_id,
    const GURL& scope,
    const GURL& script_url,
    const std::vector<ResourceRecord>& resources) {
  auto data = storage::mojom::ServiceWorkerRegistrationData::New();
  data->registration_id = registration_id;
  data->version_id = version_id;
  data->scope = scope;
  data->script = script_url;
  data->navigation_preload_state = blink::mojom::NavigationPreloadState::New();
  data->is_active = true;

  int64_t resources_total_size_bytes = 0;
  for (auto& resource : resources) {
    resources_total_size_bytes += resource->size_bytes;
  }
  data->resources_total_size_bytes = resources_total_size_bytes;

  return data;
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

// TODO(crbug.com/1016064): Remove the following helper functions to read/write
// resources once all tests that use these helper functions are moved to
// service_worker_registry_unittest.cc

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

  std::vector<int64_t> GetPurgingResources() {
    std::vector<int64_t> ids;
    base::RunLoop loop;
    storage()->GetPurgingResourceIdsForTest(base::BindLambdaForTesting(
        [&](ServiceWorkerDatabase::Status status,
            const std::vector<int64_t>& resource_ids) {
          EXPECT_EQ(status, ServiceWorkerDatabase::Status::kOk);
          ids = resource_ids;
          loop.Quit();
        }));
    loop.Run();
    return ids;
  }

  void StoreRegistrationData(
      storage::mojom::ServiceWorkerRegistrationDataPtr registration_data,
      std::vector<ResourceRecord> resources) {
    base::RunLoop loop;
    storage()->StoreRegistrationData(
        std::move(registration_data), std::move(resources),
        base::BindLambdaForTesting(
            [&](storage::mojom::ServiceWorkerDatabaseStatus status,
                int64_t /*deleted_version_id*/,
                const std::vector<int64_t>& /*newly_purgeable_resources*/) {
              ASSERT_EQ(storage::mojom::ServiceWorkerDatabaseStatus::kOk,
                        status);
              loop.Quit();
            }));
    loop.Run();
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

TEST_F(ServiceWorkerStorageTest, StoreUserData) {
  const int64_t kRegistrationId = 1;
  const GURL kScope("http://www.test.not/scope/");
  const url::Origin kOrigin = url::Origin::Create(kScope);
  const GURL kScript("http://www.test.not/script.js");
  LazyInitialize();

  // Store a registration.
  std::vector<ResourceRecord> resources;
  resources.push_back(CreateResourceRecord(1, kScript, 100));
  storage::mojom::ServiceWorkerRegistrationDataPtr registration_data =
      CreateRegistrationData(kRegistrationId,
                             /*version_id=*/1, kScope, kScript, resources);
  StoreRegistrationData(std::move(registration_data), std::move(resources));

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

  EXPECT_EQ(ServiceWorkerDatabase::Status::kOk,
            DeleteRegistrationById(kRegistrationId, kScope.GetOrigin()));
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

// Test fixture that uses disk storage, rather than memory. Useful for tests
// that test persistence by simulating browser shutdown and restart.
class ServiceWorkerStorageDiskTest : public ServiceWorkerStorageTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(InitUserDataDirectory());
    ServiceWorkerStorageTest::SetUp();
    LazyInitialize();

    // Store a registration with a resource to make sure disk cache and
    // database directories are created.
    const GURL kScope("http://www.example.com/scope/");
    const GURL kScript("http://www.example.com/script.js");
    const int64_t kScriptSize = 5;
    auto data = storage::mojom::ServiceWorkerRegistrationData::New();
    data->registration_id = 1;
    data->version_id = 1;
    data->scope = kScope;
    data->script = kScript;
    data->navigation_preload_state =
        blink::mojom::NavigationPreloadState::New();
    data->resources_total_size_bytes = kScriptSize;

    std::vector<ResourceRecord> resources;
    resources.push_back(CreateResourceRecord(1, kScript, kScriptSize));

    base::RunLoop loop;
    storage_control()->StoreRegistration(
        std::move(data), std::move(resources),
        base::BindLambdaForTesting(
            [&](storage::mojom::ServiceWorkerDatabaseStatus status) {
              DCHECK_EQ(storage::mojom::ServiceWorkerDatabaseStatus::kOk,
                        status);
              loop.Quit();
            }));
    loop.Run();

    WriteBasicResponse(storage_control(), 1);
  }
};

TEST_F(ServiceWorkerStorageDiskTest, DeleteAndStartOver) {
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

TEST_F(ServiceWorkerStorageDiskTest, DeleteAndStartOver_UnrelatedFileExists) {
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

TEST_F(ServiceWorkerStorageDiskTest, DeleteAndStartOver_OpenedFileExists) {
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

// Tests reading storage usage from database.
TEST_F(ServiceWorkerStorageTest, GetStorageUsageForOrigin) {
  const int64_t kRegistrationId1 = 1;
  const GURL kScope1("https://www.example.com/foo/");
  const GURL kScript1("https://www.example.com/foo/sw.js");
  const int64_t kRegistrationId2 = 2;
  const GURL kScope2("https://www.example.com/bar/");
  const GURL kScript2("https://www.example.com/bar/sw.js");
  const GURL kScript3("https://www.example.com/bar/sub.js");

  // Preparation: Store two registrations.
  std::vector<ResourceRecord> resources1;
  resources1.push_back(CreateResourceRecord(1, kScript1, 123));
  storage::mojom::ServiceWorkerRegistrationDataPtr data1 =
      CreateRegistrationData(
          /*registration_id=*/kRegistrationId1,
          /*version_id=*/1,
          /*scope=*/kScope1,
          /*script_url=*/kScript1, resources1);
  int64_t resources_total_size_bytes1 = data1->resources_total_size_bytes;
  StoreRegistrationData(std::move(data1), std::move(resources1));

  std::vector<ResourceRecord> resources2;
  resources2.push_back(CreateResourceRecord(2, kScript2, 456));
  resources2.push_back(CreateResourceRecord(3, kScript3, 789));
  storage::mojom::ServiceWorkerRegistrationDataPtr data2 =
      CreateRegistrationData(
          /*registration_id=*/kRegistrationId2,
          /*version_id=*/1,
          /*scope=*/kScope1,
          /*script_url=*/kScript2, resources2);
  int64_t resources_total_size_bytes2 = data2->resources_total_size_bytes;
  StoreRegistrationData(std::move(data2), std::move(resources2));

  // Storage usage should report total resource size from two registrations.
  const url::Origin origin = url::Origin::Create(kScope1.GetOrigin());
  int64_t usage;
  EXPECT_EQ(GetStorageUsageForOrigin(origin, usage),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(usage, resources_total_size_bytes1 + resources_total_size_bytes2);

  // Delete the first registration. Storage usage should report only the second
  // registration.
  EXPECT_EQ(DeleteRegistrationById(kRegistrationId1, origin.GetURL()),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetStorageUsageForOrigin(origin, usage),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(usage, resources_total_size_bytes2);

  // Delete the second registration. No storage usage should be reported.
  EXPECT_EQ(DeleteRegistrationById(kRegistrationId2, origin.GetURL()),
            ServiceWorkerDatabase::Status::kOk);
  EXPECT_EQ(GetStorageUsageForOrigin(origin, usage),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(usage, 0);
}

}  // namespace service_worker_storage_unittest
}  // namespace content
