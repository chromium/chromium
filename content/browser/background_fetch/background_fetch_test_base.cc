// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_test_base.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kTestOrigin[] = "https://example.com/";
const char kTestScriptUrl[] = "https://example.com/sw.js";

void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                              base::Closure quit_closure,
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
    base::Closure quit_closure,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK(out_service_worker_registration);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);

  *out_service_worker_registration = service_worker_registration;

  std::move(quit_closure).Run();
}

// Callback for UnregisterServiceWorker.
void DidUnregisterServiceWorker(base::Closure quit_closure,
                                blink::ServiceWorkerStatusCode status) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  std::move(quit_closure).Run();
}

GURL GetScopeForId(int64_t id) {
  return GURL(kTestOrigin + base::IntToString(id));
}

}  // namespace

BackgroundFetchTestBase::BackgroundFetchTestBase()
    // Using REAL_IO_THREAD would give better coverage for thread safety, but
    // at time of writing EmbeddedWorkerTestHelper didn't seem to support that.
    : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
      delegate_(browser_context_.GetBackgroundFetchDelegate()),
      origin_(url::Origin::Create(GURL(kTestOrigin))),
      storage_partition_(
          BrowserContext::GetDefaultStoragePartition(browser_context())) {}

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
  GURL script_url(kTestScriptUrl);
  int64_t service_worker_registration_id =
      blink::mojom::kInvalidServiceWorkerRegistrationId;

  {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GetScopeForId(next_pattern_id_++);
    base::RunLoop run_loop;
    embedded_worker_test_helper_.context()->RegisterServiceWorker(
        script_url, options,
        base::BindOnce(&DidRegisterServiceWorker,
                       &service_worker_registration_id,
                       run_loop.QuitClosure()));

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
    embedded_worker_test_helper_.context()->storage()->FindRegistrationForId(
        service_worker_registration_id, origin_.GetURL(),
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
  embedded_worker_test_helper_.context()->UnregisterServiceWorker(
      GetScopeForId(service_worker_registration_id),
      base::BindOnce(&DidUnregisterServiceWorker, run_loop.QuitClosure()));
  run_loop.Run();
}

ServiceWorkerFetchRequest
BackgroundFetchTestBase::CreateRequestWithProvidedResponse(
    const std::string& method,
    const GURL& url,
    std::unique_ptr<TestResponse> response) {
  // Register the |response| with the faked delegate.
  delegate_->RegisterResponse(url, std::move(response));

  // Create a ServiceWorkerFetchRequest request with the same information.
  return ServiceWorkerFetchRequest(url, method, ServiceWorkerHeaderMap(),
                                   Referrer(), false /* is_reload */);
}

std::unique_ptr<BackgroundFetchRegistration>
BackgroundFetchTestBase::CreateBackgroundFetchRegistration(
    const std::string& developer_id,
    const std::string& unique_id,
    blink::mojom::BackgroundFetchResult result,
    blink::mojom::BackgroundFetchFailureReason failure_reason) {
  auto registration = std::make_unique<BackgroundFetchRegistration>(
      developer_id, unique_id, 0 /* upload_total */, 0 /* uploaded */,
      0 /* download_total */, 0 /* downloaded */, result, failure_reason);
  return registration;
}

}  // namespace content
