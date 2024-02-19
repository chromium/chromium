// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_BASE_BROWSERTEST_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_BASE_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/browser/background_sync_registration.h"
#include "content/public/test/content_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace {

const char kDefaultTestURL[] = "/background_sync/test.html";
const char kEmptyURL[] = "/background_sync/empty.html";
const char kRegisterSyncFromIFrameURL[] =
    "/background_sync/register_sync_from_iframe.html";
const char kRegisterPeriodicSyncFromIFrameURL[] =
    "/background_sync/register_periodicsync_from_iframe.html";
const char kRegisterSyncFromSWURL[] =
    "/background_sync/register_sync_from_sw.html";
const char kSuccessfulOperationPrefix[] = "ok - ";

}  // namespace

namespace content {

class Shell;
class StoragePartitionImpl;
class WebContents;
struct EvalJsResult;

class BackgroundSyncBaseBrowserTest : public ContentBrowserTest {
 public:
  BackgroundSyncBaseBrowserTest();
  ~BackgroundSyncBaseBrowserTest() override;

  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  std::string BuildScriptString(const std::string& function,
                                const std::string& argument);
  std::string BuildExpectedResult(const std::string& tag,
                                  const std::string& action);

  // Returns true if the registration with tag |tag| is currently pending. Fails
  // (assertion failure) if the tag isn't registered.
  bool RegistrationPending(const std::string& tag);

  void CompleteDelayedSyncEvent();

  void SetTestClock(base::SimpleTestClock* clock);

  void ClearStoragePartitionData();

  EvalJsResult PopConsoleString();
  void RegisterServiceWorker();
  void SetIncognitoMode(bool incognito);
  WebContents* web_contents();
  void LoadTestPage(const std::string& path);
  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  void RegistrationPendingCallback(
      base::OnceClosure quit,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      bool* result_out,
      bool result);
  void RegistrationPendingDidGetSyncRegistration(
      const std::string& tag,
      base::OnceCallback<void(bool)> callback,
      BackgroundSyncStatus error_type,
      std::vector<std::unique_ptr<BackgroundSyncRegistration>> registrations);
  void RegistrationPendingDidGetSWRegistration(
      const scoped_refptr<BackgroundSyncContextImpl> sync_context,
      const std::string& tag,
      base::OnceCallback<void(bool)> callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void RegistrationPendingOnCoreThread(
      const scoped_refptr<BackgroundSyncContextImpl> sync_context,
      const scoped_refptr<ServiceWorkerContextWrapper> sw_context,
      const std::string& tag,
      const GURL& url,
      base::OnceCallback<void(bool)> callback);
  StoragePartitionImpl* GetStorage();

  raw_ptr<Shell, DanglingUntriaged> shell_ = nullptr;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_BASE_BROWSERTEST_H_
