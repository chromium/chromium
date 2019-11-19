// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include <memory>

#include "base/test/bind_test_util.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerContextCoreTest : public testing::Test,
                                     public ServiceWorkerContextCoreObserver {
 public:
  ServiceWorkerContextCoreTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override {
    if (is_observing_context_) {
      helper_->context_wrapper()->RemoveObserver(this);
      helper_.reset();
    }
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  // Runs until |registration| has an active version and it is activated.
  void RunUntilActivatedVersion(ServiceWorkerRegistration* registration) {
    if (registration->active_version() &&
        registration->active_version()->status() ==
            ServiceWorkerVersion::ACTIVATED)
      return;
    if (!is_observing_context_) {
      helper_->context_wrapper()->AddObserver(this);
      is_observing_context_ = true;
    }
    base::RunLoop loop;
    scope_for_wait_for_activated_ = registration->scope();
    quit_closure_for_wait_for_activated_ = loop.QuitClosure();
    loop.Run();
  }

  // Registers |script| and waits for the service worker to become activated.
  void RegisterServiceWorker(
      const GURL& script,
      blink::mojom::ServiceWorkerRegistrationOptions options) {
    base::RunLoop loop;
    blink::ServiceWorkerStatusCode status;
    int64_t registration_id;
    context()->RegisterServiceWorker(
        script, options, blink::mojom::FetchClientSettingsObject::New(),
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode result_status,
                const std::string& /* status_message */,
                int64_t result_registration_id) {
              status = result_status;
              registration_id = result_registration_id;
              loop.Quit();
            }));
    loop.Run();
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    scoped_refptr<ServiceWorkerRegistration> registration =
        context()->GetLiveRegistration(registration_id);
    ASSERT_TRUE(registration);
    RunUntilActivatedVersion(registration.get());
    EXPECT_TRUE(registration->active_version());
  }

  // Wrapper for ServiceWorkerStorage::FindRegistrationForScope.
  blink::ServiceWorkerStatusCode FindRegistrationForScope(const GURL& scope) {
    base::RunLoop loop;
    blink::ServiceWorkerStatusCode status;
    context()->storage()->FindRegistrationForScope(
        scope,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode result_status,
                scoped_refptr<ServiceWorkerRegistration> result_registration) {
              status = result_status;
              loop.Quit();
            }));
    loop.Run();
    return status;
  }

  // Wrapper for ServiceWorkerContextCore::DeleteForOrigin.
  blink::ServiceWorkerStatusCode DeleteForOrigin(const GURL& origin) {
    blink::ServiceWorkerStatusCode status;
    base::RunLoop loop;
    context()->DeleteForOrigin(
        origin, base::BindLambdaForTesting(
                    [&](blink::ServiceWorkerStatusCode result_status) {
                      status = result_status;
                      loop.Quit();
                    }));
    loop.Run();
    return status;
  }

 protected:
  // ServiceWorkerContextCoreObserver overrides:
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             ServiceWorkerVersion::Status status) override {
    if (status == ServiceWorkerVersion::ACTIVATED &&
        scope == scope_for_wait_for_activated_ &&
        quit_closure_for_wait_for_activated_) {
      std::move(quit_closure_for_wait_for_activated_).Run();
    }
  }

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  GURL scope_for_wait_for_activated_;
  base::OnceClosure quit_closure_for_wait_for_activated_;
  bool is_observing_context_ = false;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextCoreTest);
};

TEST_F(ServiceWorkerContextCoreTest, FailureInfo) {
  const int64_t kVersionId = 55;  // dummy value

  EXPECT_EQ(0, context()->GetVersionFailureCount(kVersionId));
  context()->UpdateVersionFailureCount(kVersionId,
                                       blink::ServiceWorkerStatusCode::kOk);
  context()->UpdateVersionFailureCount(
      kVersionId, blink::ServiceWorkerStatusCode::kErrorDisallowed);
  EXPECT_EQ(0, context()->GetVersionFailureCount(kVersionId));

  context()->UpdateVersionFailureCount(
      kVersionId, blink::ServiceWorkerStatusCode::kErrorNetwork);
  EXPECT_EQ(1, context()->GetVersionFailureCount(kVersionId));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNetwork,
            context()->failure_counts_[kVersionId].last_failure);

  context()->UpdateVersionFailureCount(
      kVersionId, blink::ServiceWorkerStatusCode::kErrorAbort);
  EXPECT_EQ(2, context()->GetVersionFailureCount(kVersionId));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort,
            context()->failure_counts_[kVersionId].last_failure);

  context()->UpdateVersionFailureCount(kVersionId,
                                       blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(0, context()->GetVersionFailureCount(kVersionId));
  EXPECT_FALSE(base::Contains(context()->failure_counts_, kVersionId));
}

TEST_F(ServiceWorkerContextCoreTest, DeleteForOrigin) {
  const GURL script("https://www.example.com/a/sw.js");
  const GURL scope("https://www.example.com/a");
  const GURL origin("https://www.example.com");

  // Register a service worker.
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;
  RegisterServiceWorker(scope, options);

  // Delete for origin.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, DeleteForOrigin(origin));

  // The registration should be deleted.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForScope(scope));
}

// Tests that DeleteForOrigin() doesn't get stuck forever even upon an error
// when trying to unregister.
TEST_F(ServiceWorkerContextCoreTest, DeleteForOrigin_UnregisterFail) {
  const GURL script("https://www.example.com/a/sw.js");
  const GURL scope("https://www.example.com/a");
  const GURL origin("https://www.example.com");

  // Register a service worker.
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;
  RegisterServiceWorker(scope, options);

  // Start DeleteForOrigin().
  base::RunLoop loop;
  blink::ServiceWorkerStatusCode status;
  context()->DeleteForOrigin(
      origin, base::BindLambdaForTesting(
                  [&](blink::ServiceWorkerStatusCode result_status) {
                    status = result_status;
                    loop.Quit();
                  }));
  // Disable storage before it finishes. This causes the Unregister job to
  // complete with an error.
  context()->storage()->Disable();
  loop.Run();

  // The operation should still complete.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed, status);
}

}  // namespace content
