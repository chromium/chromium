// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_base_browsertest.h"

#include <memory>
#include <set>
#include <vector>
#include "base/metrics/field_trial_param_associator.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/mock_background_sync_controller.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

BackgroundSyncBaseBrowserTest::BackgroundSyncBaseBrowserTest() = default;
BackgroundSyncBaseBrowserTest::~BackgroundSyncBaseBrowserTest() = default;

std::string BackgroundSyncBaseBrowserTest::BuildScriptString(
    const std::string& function,
    const std::string& argument) {
  return base::StringPrintf("%s('%s');", function.c_str(), argument.c_str());
}

std::string BackgroundSyncBaseBrowserTest::BuildExpectedResult(
    const std::string& tag,
    const std::string& action) {
  return base::StringPrintf("%s%s %s", kSuccessfulOperationPrefix, tag.c_str(),
                            action.c_str());
}

bool BackgroundSyncBaseBrowserTest::RegistrationPending(
    const std::string& tag) {
  bool is_pending;
  base::RunLoop run_loop;

  StoragePartitionImpl* storage = GetStorage();
  BackgroundSyncContextImpl* sync_context = storage->GetBackgroundSyncContext();
  ServiceWorkerContextWrapper* service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          storage->GetServiceWorkerContext());

  auto callback = base::BindOnce(
      &BackgroundSyncBaseBrowserTest::RegistrationPendingCallback,
      base::Unretained(this), run_loop.QuitClosure(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), &is_pending);

  RegistrationPendingOnCoreThread(base::WrapRefCounted(sync_context),
                                  base::WrapRefCounted(service_worker_context),
                                  tag, https_server_->GetURL(kDefaultTestURL),
                                  std::move(callback));
  run_loop.Run();

  return is_pending;
}

void BackgroundSyncBaseBrowserTest::CompleteDelayedSyncEvent() {
  ASSERT_EQ(BuildExpectedResult("delay", "completing"),
            EvalJs(web_contents(), "completeDelayedSyncEvent()"));
}

void BackgroundSyncBaseBrowserTest::RegistrationPendingCallback(
    base::OnceClosure quit,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    bool* result_out,
    bool result) {
  *result_out = result;
  task_runner->PostTask(FROM_HERE, std::move(quit));
}

void BackgroundSyncBaseBrowserTest::RegistrationPendingDidGetSyncRegistration(
    const std::string& tag,
    base::OnceCallback<void(bool)> callback,
    BackgroundSyncStatus error_type,
    std::vector<std::unique_ptr<BackgroundSyncRegistration>> registrations) {
  ASSERT_EQ(BACKGROUND_SYNC_STATUS_OK, error_type);
  // Find the right registration in the list and check its status.
  for (const auto& registration : registrations) {
    if (registration->options()->tag == tag) {
      std::move(callback).Run(registration->sync_state() ==
                              blink::mojom::BackgroundSyncState::PENDING);
      return;
    }
  }
  ADD_FAILURE() << "Registration should exist";
}

void BackgroundSyncBaseBrowserTest::RegistrationPendingDidGetSWRegistration(
    const scoped_refptr<BackgroundSyncContextImpl> sync_context,
    const std::string& tag,
    base::OnceCallback<void(bool)> callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  int64_t service_worker_id = registration->id();
  BackgroundSyncManager* sync_manager = sync_context->background_sync_manager();
  sync_manager->GetOneShotSyncRegistrations(
      service_worker_id,
      base::BindOnce(&BackgroundSyncBaseBrowserTest::
                         RegistrationPendingDidGetSyncRegistration,
                     base::Unretained(this), tag, std::move(callback)));
}

void BackgroundSyncBaseBrowserTest::RegistrationPendingOnCoreThread(
    const scoped_refptr<BackgroundSyncContextImpl> sync_context,
    const scoped_refptr<ServiceWorkerContextWrapper> sw_context,
    const std::string& tag,
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  sw_context->FindReadyRegistrationForClientUrl(
      url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      base::BindOnce(&BackgroundSyncBaseBrowserTest::
                         RegistrationPendingDidGetSWRegistration,
                     base::Unretained(this), sync_context, tag,
                     std::move(callback)));
}

void BackgroundSyncBaseBrowserTest::SetUp() {
  const char kTrialName[] = "BackgroundSync";
  const char kGroupName[] = "BackgroundSync";
  const char kFeatureName[] = "PeriodicBackgroundSync";
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  std::map<std::string, std::string> params;
  params["max_sync_attempts"] = "1";
  params["min_periodic_sync_events_interval_sec"] = "5";
  base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kGroupName, params);
  std::unique_ptr<base::FeatureList> feature_list(
      std::make_unique<base::FeatureList>());
  feature_list->RegisterFieldTrialOverride(
      kFeatureName, base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

  background_sync_test_util::SetIgnoreNetworkChanges(true);
  ContentBrowserTest::SetUp();
}

void BackgroundSyncBaseBrowserTest::SetIncognitoMode(bool incognito) {
  shell_ = incognito ? CreateOffTheRecordBrowser() : shell();
  // Let any async shell creation logic finish.
  base::RunLoop().RunUntilIdle();
}

StoragePartitionImpl* BackgroundSyncBaseBrowserTest::GetStorage() {
  WebContents* web_contents = shell_->web_contents();
  return static_cast<StoragePartitionImpl*>(
      web_contents->GetBrowserContext()->GetStoragePartition(
          web_contents->GetSiteInstance()));
}

WebContents* BackgroundSyncBaseBrowserTest::web_contents() {
  return shell_->web_contents();
}

void BackgroundSyncBaseBrowserTest::SetUpOnMainThread() {
  https_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_->ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server_->Start());

  SetIncognitoMode(false);
  background_sync_test_util::SetOnline(web_contents(), true);
  LoadTestPage(kDefaultTestURL);

  ContentBrowserTest::SetUpOnMainThread();
}

void BackgroundSyncBaseBrowserTest::TearDownOnMainThread() {
  https_server_.reset();
}

void BackgroundSyncBaseBrowserTest::LoadTestPage(const std::string& path) {
  ASSERT_TRUE(NavigateToURL(shell_, https_server_->GetURL(path)));
}

void BackgroundSyncBaseBrowserTest::SetTestClock(base::SimpleTestClock* clock) {
  DCHECK(clock);
  StoragePartitionImpl* storage = GetStorage();
  BackgroundSyncContextImpl* sync_context = storage->GetBackgroundSyncContext();

  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  background_sync_manager->set_clock(clock);
}

void BackgroundSyncBaseBrowserTest::ClearStoragePartitionData() {
  // Clear data from the storage partition.  Parameters are set to clear data
  // for service workers, for all origins, for an unbounded time range.
  StoragePartitionImpl* storage = GetStorage();

  uint32_t storage_partition_mask =
      StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  uint32_t quota_storage_mask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  blink::StorageKey delete_storage_key = blink::StorageKey();
  const base::Time delete_begin = base::Time();
  base::Time delete_end = base::Time::Max();

  base::RunLoop run_loop;

  storage->ClearData(storage_partition_mask, quota_storage_mask,
                     delete_storage_key, delete_begin, delete_end,
                     run_loop.QuitClosure());

  run_loop.Run();
}

EvalJsResult BackgroundSyncBaseBrowserTest::PopConsoleString() {
  return EvalJs(web_contents(), "resultQueue.pop()");
}

void BackgroundSyncBaseBrowserTest::RegisterServiceWorker() {
  ASSERT_EQ(BuildExpectedResult("service worker", "registered"),
            EvalJs(web_contents(), "registerServiceWorker()"));
}

}  // namespace content
