// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_base.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kTestOrigin[] = "https://example.com/";

void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                              base::OnceClosure quit_closure,
                              blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t service_worker_registration_id) {
  DCHECK(out_service_worker_registration_id);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status) << status_message;

  *out_service_worker_registration_id = service_worker_registration_id;

  std::move(quit_closure).Run();
}

void DidFindServiceWorkerRegistration(
    scoped_refptr<ServiceWorkerRegistration>* out_service_worker_registration,
    base::OnceClosure quit_closure,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK(out_service_worker_registration);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);

  *out_service_worker_registration = service_worker_registration;

  std::move(quit_closure).Run();
}

// Callback for UnregisterServiceWorker.
void DidUnregisterServiceWorker(base::OnceClosure quit_closure,
                                blink::ServiceWorkerStatusCode status) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  std::move(quit_closure).Run();
}

GURL GetScopeForId(const std::string& origin, int64_t id) {
  return GURL(origin + base::NumberToString(id));
}

}  // namespace

BackgroundFetchTestBase::BackgroundFetchTestBase()
    // Using REAL_IO_THREAD would give better coverage for thread safety, but
    // at time of writing EmbeddedWorkerTestHelper didn't seem to support that.
    : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
      delegate_(browser_context_.GetBackgroundFetchDelegate()),
      embedded_worker_test_helper_(base::FilePath()),
      storage_key_(blink::StorageKey::CreateFirstParty(
          url::Origin::Create(GURL(kTestOrigin)))),
      storage_partition_factory_(static_cast<StoragePartitionImpl*>(
          browser_context()->GetDefaultStoragePartition())) {}

BackgroundFetchTestBase::~BackgroundFetchTestBase() {
  DCHECK(set_up_called_);
  DCHECK(tear_down_called_);
}

void BackgroundFetchTestBase::SetUp() {
  set_up_called_ = true;
}

void BackgroundFetchTestBase::TearDown() {
  service_worker_registrations_.clear();
  tear_down_called_ = true;
}

int64_t BackgroundFetchTestBase::RegisterServiceWorker() {
  return RegisterServiceWorkerForOrigin(storage_key_.origin());
}

int64_t BackgroundFetchTestBase::RegisterServiceWorkerForOrigin(
    const url::Origin& origin) {
  GURL script_url(origin.GetURL().spec() + "sw.js");
  int64_t service_worker_registration_id =
      blink::mojom::kInvalidServiceWorkerRegistrationId;

  const blink::StorageKey key = blink::StorageKey::CreateFirstParty(origin);

  {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GetScopeForId(origin.GetURL().spec(), next_pattern_id_++);
    base::RunLoop run_loop;
    embedded_worker_test_helper_.context()->RegisterServiceWorker(
        script_url, key, options,
        blink::mojom::FetchClientSettingsObject::New(),
        base::BindOnce(&DidRegisterServiceWorker,
                       &service_worker_registration_id, run_loop.QuitClosure()),
        /*requesting_frame_id=*/GlobalRenderFrameHostId(),
        PolicyContainerPolicies());

    run_loop.Run();
  }

  if (service_worker_registration_id ==
      blink::mojom::kInvalidServiceWorkerRegistrationId) {
    ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
    return blink::mojom::kInvalidServiceWorkerRegistrationId;
  }

  scoped_refptr<ServiceWorkerRegistration> service_worker_registration;

  {
    base::RunLoop run_loop;
    embedded_worker_test_helper_.context()->registry()->FindRegistrationForId(
        service_worker_registration_id, key,
        base::BindOnce(&DidFindServiceWorkerRegistration,
                       &service_worker_registration, run_loop.QuitClosure()));

    run_loop.Run();
  }

  // Wait for the worker to be activated.
  base::RunLoop().RunUntilIdle();

  if (!service_worker_registration) {
    ADD_FAILURE() << "Could not find the new Service Worker registration.";
    return blink::mojom::kInvalidServiceWorkerRegistrationId;
  }

  service_worker_registrations_.push_back(
      std::move(service_worker_registration));

  return service_worker_registration_id;
}

void BackgroundFetchTestBase::UnregisterServiceWorker(
    int64_t service_worker_registration_id) {
  base::RunLoop run_loop;
  const GURL scope = GetScopeForId(kTestOrigin, service_worker_registration_id);
  embedded_worker_test_helper_.context()->UnregisterServiceWorker(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      /*is_immediate=*/false,
      base::BindOnce(&DidUnregisterServiceWorker, run_loop.QuitClosure()));
  run_loop.Run();
}

blink::mojom::FetchAPIRequestPtr
BackgroundFetchTestBase::CreateRequestWithProvidedResponse(
    const std::string& method,
    const GURL& url,
    std::unique_ptr<TestResponse> response) {
  // Register the |response| with the faked delegate.
  delegate_->RegisterResponse(url, std::move(response));

  // Create a blink::mojom::FetchAPIRequestPtr request with the same
  // information.
  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = url;
  request->method = method;
  request->is_reload = false;
  request->referrer = blink::mojom::Referrer::New();
  request->headers = {};
  return request;
}

blink::mojom::BackgroundFetchRegistrationDataPtr
BackgroundFetchTestBase::CreateBackgroundFetchRegistrationData(
    const std::string& developer_id,
    blink::mojom::BackgroundFetchResult result,
    blink::mojom::BackgroundFetchFailureReason failure_reason) {
  return blink::mojom::BackgroundFetchRegistrationData::New(
      developer_id, /* upload_total= */ 0, /* uploaded= */ 0,
      /* download_total= */ 0, /* downloaded= */ 0, result, failure_reason);
}

DevToolsBackgroundServicesContextImpl&
BackgroundFetchTestBase::devtools_context() {
  return CHECK_DEREF(static_cast<DevToolsBackgroundServicesContextImpl*>(
      storage_partition()->GetDevToolsBackgroundServicesContext()));
}

}  // namespace content
