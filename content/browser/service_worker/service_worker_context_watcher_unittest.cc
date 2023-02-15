// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_watcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {

namespace {

void DidRegisterServiceWorker(int64_t* registration_id_out,
                              blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t registration_id) {
  ASSERT_TRUE(registration_id_out);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  *registration_id_out = registration_id;
}

void DidUnregisterServiceWorker(blink::ServiceWorkerStatusCode* status_out,
                                blink::ServiceWorkerStatusCode status) {
  ASSERT_TRUE(status_out);
  *status_out = status;
}

class WatcherCallback {
 public:
  WatcherCallback() {}

  WatcherCallback(const WatcherCallback&) = delete;
  WatcherCallback& operator=(const WatcherCallback&) = delete;

  ~WatcherCallback() {}

  scoped_refptr<ServiceWorkerContextWatcher> StartWatch(
      scoped_refptr<ServiceWorkerContextWrapper> context) {
    scoped_refptr<ServiceWorkerContextWatcher> watcher =
        base::MakeRefCounted<ServiceWorkerContextWatcher>(
            context,
            base::BindRepeating(&WatcherCallback::OnRegistrationUpdated,
                                weak_factory_.GetWeakPtr()),
            base::BindRepeating(&WatcherCallback::OnVersionUpdated,
                                weak_factory_.GetWeakPtr()),
            base::BindRepeating(&WatcherCallback::OnErrorReported,
                                weak_factory_.GetWeakPtr()));
    watcher->Start();
    return watcher;
  }

  const std::map<int64_t, ServiceWorkerRegistrationInfo>& registrations()
      const {
    return registrations_;
  }

  const std::map<int64_t, std::map<int64_t, ServiceWorkerVersionInfo>>&
  versions() const {
    return versions_;
  }
  const std::map<
      int64_t,
      std::map<int64_t, std::vector<ServiceWorkerContextObserver::ErrorInfo>>>&
  errors() const {
    return errors_;
  }

  int callback_count() const { return callback_count_; }

 private:
  void OnRegistrationUpdated(
      const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
    ++callback_count_;
    for (const auto& info : registrations) {
      if (info.delete_flag ==
          ServiceWorkerRegistrationInfo::DeleteFlag::IS_DELETED) {
        registrations_.erase(info.registration_id);
      } else {
        registrations_[info.registration_id] = info;
      }
    }
  }

  void OnVersionUpdated(const std::vector<ServiceWorkerVersionInfo>& versions) {
    ++callback_count_;
    for (const auto& info : versions) {
      versions_[info.registration_id][info.version_id] = info;
    }
  }

  void OnErrorReported(
      int64_t registration_id,
      int64_t version_id,
      const ServiceWorkerContextObserver::ErrorInfo& error_info) {
    ++callback_count_;
    errors_[registration_id][version_id].push_back(error_info);
  }

  std::map<int64_t /* registration_id */, ServiceWorkerRegistrationInfo>
      registrations_;
  std::map<int64_t /* registration_id */,
           std::map<int64_t /* version_id */, ServiceWorkerVersionInfo>>
      versions_;
  std::map<int64_t /* registration_id */,
           std::map<int64_t /* version_id */,
                    std::vector<ServiceWorkerContextObserver::ErrorInfo>>>
      errors_;

  int callback_count_ = 0;

  base::WeakPtrFactory<WatcherCallback> weak_factory_{this};
};

}  // namespace

class ServiceWorkerContextWatcherTest : public testing::Test {
 public:
  ServiceWorkerContextWatcherTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  ServiceWorkerContextWatcherTest(const ServiceWorkerContextWatcherTest&) =
      delete;
  ServiceWorkerContextWatcherTest& operator=(
      const ServiceWorkerContextWatcherTest&) = delete;

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    helper_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }
  int64_t RegisterServiceWorker(const GURL& scope,
                                const GURL& script_url,
                                const blink::StorageKey& key) {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
    context()->RegisterServiceWorker(
        script_url, key, options,
        blink::mojom::FetchClientSettingsObject::New(),
        base::BindOnce(&DidRegisterServiceWorker, &registration_id),
        /*requesting_frame_id=*/GlobalRenderFrameHostId(),
        PolicyContainerPolicies());
    base::RunLoop().RunUntilIdle();
    return registration_id;
  }

  blink::ServiceWorkerStatusCode UnregisterServiceWorker(
      const GURL& scope,
      const blink::StorageKey& key) {
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    context()->UnregisterServiceWorker(
        scope, key, /*is_immediate=*/false,
        base::BindOnce(&DidUnregisterServiceWorker, &status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void ReportError(scoped_refptr<ServiceWorkerContextWatcher> watcher,
                   int64_t version_id,
                   const GURL& scope,
                   const blink::StorageKey& key,
                   const ServiceWorkerContextObserver::ErrorInfo& error_info) {
    watcher->OnErrorReported(version_id, scope, key, error_info);
  }

 private:
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  BrowserTaskEnvironment task_environment_;
};

TEST_F(ServiceWorkerContextWatcherTest, NoServiceWorker) {
  WatcherCallback watcher_callback;
  scoped_refptr<ServiceWorkerContextWatcher> watcher =
      watcher_callback.StartWatch(context_wrapper());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, watcher_callback.registrations().size());
  EXPECT_EQ(0u, watcher_callback.versions().size());
  EXPECT_EQ(0u, watcher_callback.errors().size());
  // OnRegistrationUpdated() and OnVersionUpdated() must be called with the
  // empty initial data.
  EXPECT_EQ(2, watcher_callback.callback_count());
  watcher->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, watcher_callback.registrations().size());
  EXPECT_EQ(0u, watcher_callback.versions().size());
  EXPECT_EQ(2, watcher_callback.callback_count());
}

TEST_F(ServiceWorkerContextWatcherTest, StoredServiceWorkers) {
  GURL scope_1 = GURL("https://www1.example.com/");
  GURL script_1 = GURL("https://www1.example.com/worker.js");
  const blink::StorageKey key_1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_1));
  int64_t registration_id_1 = RegisterServiceWorker(scope_1, script_1, key_1);
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration_id_1);

  GURL scope_2 = GURL("https://www2.example.com/");
  GURL script_2 = GURL("https://www2.example.com/worker.js");
  const blink::StorageKey key_2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_2));
  int64_t registration_id_2 = RegisterServiceWorker(scope_2, script_2, key_2);
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration_id_2);

  WatcherCallback watcher_callback;
  scoped_refptr<ServiceWorkerContextWatcher> watcher =
      watcher_callback.StartWatch(context_wrapper());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, watcher_callback.registrations().size());
  EXPECT_EQ(scope_1,
            watcher_callback.registrations().at(registration_id_1).scope);
  EXPECT_EQ(key_1, watcher_callback.registrations().at(registration_id_1).key);
  EXPECT_EQ(scope_2,
            watcher_callback.registrations().at(registration_id_2).scope);
  EXPECT_EQ(key_2, watcher_callback.registrations().at(registration_id_2).key);
  ASSERT_EQ(2u, watcher_callback.versions().size());
  EXPECT_EQ(script_1, watcher_callback.versions()
                          .at(registration_id_1)
                          .begin()
                          ->second.script_url);
  EXPECT_EQ(script_2, watcher_callback.versions()
                          .at(registration_id_2)
                          .begin()
                          ->second.script_url);
  EXPECT_EQ(0u, watcher_callback.errors().size());

  watcher->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerContextWatcherTest, RegisteredServiceWorker) {
  GURL scope_1 = GURL("https://www1.example.com/");
  GURL script_1 = GURL("https://www1.example.com/worker.js");
  const blink::StorageKey key_1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_1));
  int64_t registration_id_1 = RegisterServiceWorker(scope_1, script_1, key_1);
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration_id_1);

  WatcherCallback watcher_callback;
  scoped_refptr<ServiceWorkerContextWatcher> watcher =
      watcher_callback.StartWatch(context_wrapper());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, watcher_callback.registrations().size());
  EXPECT_EQ(scope_1,
            watcher_callback.registrations().at(registration_id_1).scope);
  EXPECT_EQ(key_1, watcher_callback.registrations().at(registration_id_1).key);
  ASSERT_EQ(1u, watcher_callback.versions().size());
  EXPECT_EQ(script_1, watcher_callback.versions()
                          .at(registration_id_1)
                          .begin()
                          ->second.script_url);
  EXPECT_EQ(0u, watcher_callback.errors().size());

  GURL scope_2 = GURL("https://www2.example.com/");
  GURL script_2 = GURL("https://www2.example.com/worker.js");
  const blink::StorageKey key_2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_2));
  int64_t registration_id_2 = RegisterServiceWorker(scope_2, script_2, key_2);
  ASSERT_EQ(2u, watcher_callback.registrations().size());
  EXPECT_EQ(scope_1,
            watcher_callback.registrations().at(registration_id_1).scope);
  EXPECT_EQ(key_1, watcher_callback.registrations().at(registration_id_1).key);
  EXPECT_EQ(scope_2,
            watcher_callback.registrations().at(registration_id_2).scope);
  EXPECT_EQ(key_2, watcher_callback.registrations().at(registration_id_2).key);
  ASSERT_EQ(2u, watcher_callback.versions().size());
  EXPECT_EQ(script_1, watcher_callback.versions()
                          .at(registration_id_1)
                          .begin()
                          ->second.script_url);
  EXPECT_EQ(script_2, watcher_callback.versions()
                          .at(registration_id_2)
                          .begin()
                          ->second.script_url);
  EXPECT_EQ(0u, watcher_callback.errors().size());

  watcher->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerContextWatcherTest, UnregisteredServiceWorker) {
  GURL scope_1 = GURL("https://www1.example.com/");
  GURL script_1 = GURL("https://www1.example.com/worker.js");
  const blink::StorageKey key_1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_1));
  int64_t registration_id_1 = RegisterServiceWorker(scope_1, script_1, key_1);
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration_id_1);

  GURL scope_2 = GURL("https://www2.example.com/");
  GURL script_2 = GURL("https://www2.example.com/worker.js");
  const blink::StorageKey key_2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_2));
  int64_t registration_id_2 = RegisterServiceWorker(scope_2, script_2, key_2);

  WatcherCallback watcher_callback;
  scoped_refptr<ServiceWorkerContextWatcher> watcher =
      watcher_callback.StartWatch(context_wrapper());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, watcher_callback.registrations().size());
  EXPECT_EQ(scope_1,
            watcher_callback.registrations().at(registration_id_1).scope);
  EXPECT_EQ(key_1, watcher_callback.registrations().at(registration_id_1).key);
  EXPECT_EQ(scope_2,
            watcher_callback.registrations().at(registration_id_2).scope);
  EXPECT_EQ(key_2, watcher_callback.registrations().at(registration_id_2).key);
  ASSERT_EQ(2u, watcher_callback.versions().size());

  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            UnregisterServiceWorker(scope_1, key_1));

  ASSERT_EQ(1u, watcher_callback.registrations().size());
  EXPECT_EQ(scope_2,
            watcher_callback.registrations().at(registration_id_2).scope);
  EXPECT_EQ(key_2, watcher_callback.registrations().at(registration_id_2).key);

  watcher->Stop();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerContextWatcherTest, ErrorReport) {
  GURL scope = GURL("https://www1.example.com/");
  GURL script = GURL("https://www1.example.com/worker.js");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  int64_t registration_id = RegisterServiceWorker(scope, script, key);
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);

  WatcherCallback watcher_callback;
  scoped_refptr<ServiceWorkerContextWatcher> watcher =
      watcher_callback.StartWatch(context_wrapper());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, watcher_callback.registrations().size());
  EXPECT_EQ(scope, watcher_callback.registrations().at(registration_id).scope);
  EXPECT_EQ(key, watcher_callback.registrations().at(registration_id).key);
  ASSERT_EQ(1u, watcher_callback.versions().size());
  EXPECT_EQ(script, watcher_callback.versions()
                        .at(registration_id)
                        .begin()
                        ->second.script_url);
  int64_t version_id =
      watcher_callback.versions().at(registration_id).begin()->first;
  EXPECT_EQ(0u, watcher_callback.errors().size());

  std::u16string message(u"HELLO");
  ReportError(watcher, version_id, scope, key,
              ServiceWorkerContextObserver::ErrorInfo(message, 0, 0, script));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, watcher_callback.errors().size());
  ASSERT_EQ(1u, watcher_callback.errors().at(registration_id).size());
  ASSERT_EQ(
      1u, watcher_callback.errors().at(registration_id).at(version_id).size());
  EXPECT_EQ(message, watcher_callback.errors()
                         .at(registration_id)
                         .at(version_id)[0]
                         .error_message);

  watcher->Stop();
  base::RunLoop().RunUntilIdle();
}

// This test checks that even if ServiceWorkerContextWatcher::Stop() is called
// quickly after Start() is called, the crash (crbug.com/727877) should not
// happen.
TEST_F(ServiceWorkerContextWatcherTest, StopQuickly) {
  WatcherCallback watcher_callback;
  scoped_refptr<ServiceWorkerContextWatcher> watcher =
      watcher_callback.StartWatch(context_wrapper());
  watcher->Stop();

  int callback_count = watcher_callback.callback_count();
  GURL scope = GURL("https://www1.example.com/");
  GURL script = GURL("https://www1.example.com/worker.js");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  int64_t registration_id = RegisterServiceWorker(scope, script, key);
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback_count, watcher_callback.callback_count());
}

// This test checks that any callbacks should not be executed after
// ServiceWorkerContextWatcher::Stop() is called.
TEST_F(ServiceWorkerContextWatcherTest, Race) {
  GURL scope = GURL("https://www1.example.com/");
  GURL script = GURL("https://www1.example.com/worker.js");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  int64_t registration_id = RegisterServiceWorker(scope, script, key);
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);
  base::RunLoop().RunUntilIdle();

  WatcherCallback watcher_callback;
  scoped_refptr<ServiceWorkerContextWatcher> watcher =
      watcher_callback.StartWatch(context_wrapper());
  base::RunLoop().RunUntilIdle();
  watcher->Stop();

  int callback_count = watcher_callback.callback_count();
  std::u16string message(u"HELLO");
  ReportError(watcher, 0 /*version_id*/, scope, key,
              ServiceWorkerContextObserver::ErrorInfo(message, 0, 0, script));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback_count, watcher_callback.callback_count());
}

}  // namespace content
