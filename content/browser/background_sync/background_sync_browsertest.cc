// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/background_sync/background_sync_context.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/background_sync/background_sync_status.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::NetworkChangeNotifier;

namespace content {

namespace {

const char kDefaultTestURL[] = "/background_sync/test.html";
const char kEmptyURL[] = "/background_sync/empty.html";
const char kRegisterSyncFromIFrameURL[] =
    "/background_sync/register_sync_from_iframe.html";
const char kRegisterSyncFromSWURL[] =
    "/background_sync/register_sync_from_sw.html";

const char kSuccessfulOperationPrefix[] = "ok - ";

std::string BuildScriptString(const std::string& function,
                              const std::string& argument) {
  return base::StringPrintf("%s('%s');", function.c_str(), argument.c_str());
}

std::string BuildExpectedResult(const std::string& tag,
                                const std::string& action) {
  return base::StringPrintf("%s%s %s", kSuccessfulOperationPrefix, tag.c_str(),
                            action.c_str());
}

void RegistrationPendingCallback(
    const base::Closure& quit,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    bool* result_out,
    bool result) {
  *result_out = result;
  task_runner->PostTask(FROM_HERE, quit);
}

void RegistrationPendingDidGetSyncRegistration(
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

void RegistrationPendingDidGetSWRegistration(
    const scoped_refptr<BackgroundSyncContext> sync_context,
    const std::string& tag,
    base::OnceCallback<void(bool)> callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  int64_t service_worker_id = registration->id();
  BackgroundSyncManager* sync_manager = sync_context->background_sync_manager();
  sync_manager->GetRegistrations(
      service_worker_id,
      base::BindOnce(&RegistrationPendingDidGetSyncRegistration, tag,
                     std::move(callback)));
}

void RegistrationPendingOnIOThread(
    const scoped_refptr<BackgroundSyncContext> sync_context,
    const scoped_refptr<ServiceWorkerContextWrapper> sw_context,
    const std::string& tag,
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  sw_context->FindReadyRegistrationForDocument(
      url, base::BindOnce(&RegistrationPendingDidGetSWRegistration,
                          sync_context, tag, std::move(callback)));
}

void SetMaxSyncAttemptsOnIOThread(
    const scoped_refptr<BackgroundSyncContext>& sync_context,
    int max_sync_attempts) {
  BackgroundSyncManager* background_sync_manager =
      sync_context->background_sync_manager();
  background_sync_manager->SetMaxSyncAttemptsForTesting(max_sync_attempts);
}

}  // namespace

class BackgroundSyncBrowserTest : public ContentBrowserTest {
 public:
  BackgroundSyncBrowserTest() {}
  ~BackgroundSyncBrowserTest() override {}

  void SetUp() override {
    background_sync_test_util::SetIgnoreNetworkChanges(true);

    ContentBrowserTest::SetUp();
  }

  void SetIncognitoMode(bool incognito) {
    shell_ = incognito ? CreateOffTheRecordBrowser() : shell();
    // Let any async shell creation logic finish.
    base::RunLoop().RunUntilIdle();
  }

  StoragePartitionImpl* GetStorage() {
    WebContents* web_contents = shell_->web_contents();
    return static_cast<StoragePartitionImpl*>(
        BrowserContext::GetStoragePartition(web_contents->GetBrowserContext(),
                                            web_contents->GetSiteInstance()));
  }

  BackgroundSyncContext* GetSyncContext() {
    return GetStorage()->GetBackgroundSyncContext();
  }

  WebContents* web_contents() { return shell_->web_contents(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(jkarlin): Remove this once background sync is no longer
    // experimental.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_->Start());

    SetIncognitoMode(false);
    SetMaxSyncAttempts(1);
    background_sync_test_util::SetOnline(web_contents(), true);
    ASSERT_TRUE(LoadTestPage(kDefaultTestURL));

    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { https_server_.reset(); }

  bool LoadTestPage(const std::string& path) {
    return NavigateToURL(shell_, https_server_->GetURL(path));
  }

  bool RunScript(const std::string& script, std::string* result) {
    return content::ExecuteScriptAndExtractString(web_contents(), script,
                                                  result);
  }

  // Returns true if the registration with tag |tag| is currently pending. Fails
  // (assertion failure) if the tag isn't registered.
  bool RegistrationPending(const std::string& tag);

  // Sets the BackgroundSyncManager's max sync attempts per registration.
  void SetMaxSyncAttempts(int max_sync_attempts);

  void ClearStoragePartitionData();

  std::string PopConsoleString();
  bool PopConsole(const std::string& expected_msg);
  bool RegisterServiceWorker();
  bool Register(const std::string& tag);
  bool RegisterFromServiceWorker(const std::string& tag);
  bool RegisterFromCrossOriginFrame(const std::string& frame_url,
                                    std::string* script_result);
  bool HasTag(const std::string& tag);
  bool HasTagFromServiceWorker(const std::string& tag);
  bool MatchTags(const std::string& script_result,
                 const std::vector<std::string>& expected_tags);
  bool GetTags(const std::vector<std::string>& expected_tags);
  bool GetTagsFromServiceWorker(const std::vector<std::string>& expected_tags);
  bool CompleteDelayedSyncEvent();
  bool RejectDelayedSyncEvent();

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  Shell* shell_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncBrowserTest);
};

bool BackgroundSyncBrowserTest::RegistrationPending(const std::string& tag) {
  bool is_pending;
  base::RunLoop run_loop;

  StoragePartitionImpl* storage = GetStorage();
  BackgroundSyncContext* sync_context = storage->GetBackgroundSyncContext();
  ServiceWorkerContextWrapper* service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          storage->GetServiceWorkerContext());

  auto callback =
      base::BindOnce(&RegistrationPendingCallback, run_loop.QuitClosure(),
                     base::ThreadTaskRunnerHandle::Get(), &is_pending);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &RegistrationPendingOnIOThread, base::WrapRefCounted(sync_context),
          base::WrapRefCounted(service_worker_context), tag,
          https_server_->GetURL(kDefaultTestURL), std::move(callback)));

  run_loop.Run();

  return is_pending;
}

void BackgroundSyncBrowserTest::SetMaxSyncAttempts(int max_sync_attempts) {
  base::RunLoop run_loop;

  StoragePartitionImpl* storage = GetStorage();
  BackgroundSyncContext* sync_context = storage->GetBackgroundSyncContext();

  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SetMaxSyncAttemptsOnIOThread,
                     base::WrapRefCounted(sync_context), max_sync_attempts),
      run_loop.QuitClosure());

  run_loop.Run();
}

void BackgroundSyncBrowserTest::ClearStoragePartitionData() {
  // Clear data from the storage partition.  Parameters are set to clear data
  // for service workers, for all origins, for an unbounded time range.
  StoragePartitionImpl* storage = GetStorage();

  uint32_t storage_partition_mask =
      StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  uint32_t quota_storage_mask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  const GURL& delete_origin = GURL();
  const base::Time delete_begin = base::Time();
  base::Time delete_end = base::Time::Max();

  base::RunLoop run_loop;

  storage->ClearData(storage_partition_mask, quota_storage_mask, delete_origin,
                     delete_begin, delete_end, run_loop.QuitClosure());

  run_loop.Run();
}

std::string BackgroundSyncBrowserTest::PopConsoleString() {
  std::string script_result;
  EXPECT_TRUE(RunScript("resultQueue.pop()", &script_result));
  return script_result;
}

bool BackgroundSyncBrowserTest::PopConsole(const std::string& expected_msg) {
  std::string script_result = PopConsoleString();
  return script_result == expected_msg;
}

bool BackgroundSyncBrowserTest::RegisterServiceWorker() {
  std::string script_result;
  EXPECT_TRUE(RunScript("registerServiceWorker()", &script_result));
  return script_result == BuildExpectedResult("service worker", "registered");
}

bool BackgroundSyncBrowserTest::Register(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(BuildScriptString("register", tag), &script_result));
  return script_result == BuildExpectedResult(tag, "registered");
}

bool BackgroundSyncBrowserTest::RegisterFromServiceWorker(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(BuildScriptString("registerFromServiceWorker", tag),
                        &script_result));
  return script_result == BuildExpectedResult(tag, "register sent to SW");
}

bool BackgroundSyncBrowserTest::RegisterFromCrossOriginFrame(
    const std::string& frame_url,
    std::string* script_result) {
  // Start a second https server to use as a second origin.
  net::EmbeddedTestServer alt_server(net::EmbeddedTestServer::TYPE_HTTPS);
  alt_server.ServeFilesFromSourceDirectory("content/test/data");
  EXPECT_TRUE(alt_server.Start());

  GURL url = alt_server.GetURL(frame_url);
  return RunScript(
      BuildScriptString("registerFromCrossOriginFrame", url.spec()),
      script_result);
}

bool BackgroundSyncBrowserTest::HasTag(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(BuildScriptString("hasTag", tag), &script_result));
  return script_result == BuildExpectedResult(tag, "found");
}

bool BackgroundSyncBrowserTest::HasTagFromServiceWorker(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(RunScript(BuildScriptString("hasTagFromServiceWorker", tag),
                        &script_result));
  EXPECT_TRUE(script_result == "ok - hasTag sent to SW");

  return PopConsole(BuildExpectedResult(tag, "found"));
}

bool BackgroundSyncBrowserTest::MatchTags(
    const std::string& script_result,
    const std::vector<std::string>& expected_tags) {
  EXPECT_TRUE(base::StartsWith(script_result, kSuccessfulOperationPrefix,
                               base::CompareCase::INSENSITIVE_ASCII));
  std::string tag_string =
      script_result.substr(strlen(kSuccessfulOperationPrefix));
  std::vector<std::string> result_tags = base::SplitString(
      tag_string, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  return std::set<std::string>(expected_tags.begin(), expected_tags.end()) ==
         std::set<std::string>(result_tags.begin(), result_tags.end());
}

bool BackgroundSyncBrowserTest::GetTags(
    const std::vector<std::string>& expected_tags) {
  std::string script_result;
  EXPECT_TRUE(RunScript("getTags()", &script_result));

  return MatchTags(script_result, expected_tags);
}

bool BackgroundSyncBrowserTest::GetTagsFromServiceWorker(
    const std::vector<std::string>& expected_tags) {
  std::string script_result;
  EXPECT_TRUE(RunScript("getTagsFromServiceWorker()", &script_result));
  EXPECT_TRUE(script_result == "ok - getTags sent to SW");

  return MatchTags(PopConsoleString(), expected_tags);
}

bool BackgroundSyncBrowserTest::CompleteDelayedSyncEvent() {
  std::string script_result;
  EXPECT_TRUE(RunScript("completeDelayedSyncEvent()", &script_result));
  return script_result == BuildExpectedResult("delay", "completing");
}

bool BackgroundSyncBrowserTest::RejectDelayedSyncEvent() {
  std::string script_result;
  EXPECT_TRUE(RunScript("rejectDelayedSyncEvent()", &script_result));
  return script_result == BuildExpectedResult("delay", "rejecting");
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       RegisterFromControlledDocument) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(Register("foo"));
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(HasTag("foo"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       RegisterFromUncontrolledDocument) {
  EXPECT_TRUE(RegisterServiceWorker());

  EXPECT_TRUE(Register("foo"));
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(HasTag("foo"));
}

// Verify that Register works in a service worker
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, RegisterFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(RegisterFromServiceWorker("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(PopConsole("foo_sw fired"));
  EXPECT_FALSE(HasTag("foo_sw"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       RegistrationDelaysForNetwork) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  // Prevent firing by going offline.
  background_sync_test_util::SetOnline(web_contents(), false);
  EXPECT_TRUE(Register("foo"));
  EXPECT_TRUE(HasTag("foo"));
  EXPECT_TRUE(RegistrationPending("foo"));

  // Resume firing by going online.
  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(HasTag("foo"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, WaitUntil) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(Register("delay"));

  // Verify that it is firing.
  EXPECT_TRUE(HasTag("delay"));
  EXPECT_FALSE(RegistrationPending("delay"));

  // Complete the task.
  EXPECT_TRUE(CompleteDelayedSyncEvent());
  EXPECT_TRUE(PopConsole("ok - delay completed"));

  // Verify that it finished firing.
  EXPECT_FALSE(HasTag("delay"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, WaitUntilReject) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(Register("delay"));

  // Verify that it is firing.
  EXPECT_TRUE(HasTag("delay"));
  EXPECT_FALSE(RegistrationPending("delay"));

  // Complete the task.
  EXPECT_TRUE(RejectDelayedSyncEvent());
  EXPECT_TRUE(PopConsole("ok - delay rejected"));
  EXPECT_FALSE(HasTag("delay"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, Incognito) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), false);
  EXPECT_TRUE(Register("normal"));
  EXPECT_TRUE(RegistrationPending("normal"));

  // Go incognito and verify that incognito doesn't see the registration.
  SetIncognitoMode(true);

  // Tell the new network observer that we're offline (it initializes from
  // NetworkChangeNotifier::GetCurrentConnectionType() which is not mocked out
  // in this test).
  background_sync_test_util::SetOnline(web_contents(), false);

  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));
  EXPECT_TRUE(RegisterServiceWorker());

  EXPECT_FALSE(HasTag("normal"));

  EXPECT_TRUE(Register("incognito"));
  EXPECT_TRUE(RegistrationPending("incognito"));

  // Switch back and make sure the registration is still there.
  SetIncognitoMode(false);
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Should be controlled.

  EXPECT_TRUE(HasTag("normal"));
  EXPECT_FALSE(HasTag("incognito"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, GetTags) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo");
  registered_tags.push_back("bar");

  for (const std::string& tag : registered_tags)
    EXPECT_TRUE(Register(tag));

  EXPECT_TRUE(GetTags(registered_tags));
}

// Verify that GetRegistrations works in a service worker
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       GetRegistrationsFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo_sw");
  registered_tags.push_back("bar_sw");

  for (const std::string& tag : registered_tags) {
    EXPECT_TRUE(RegisterFromServiceWorker(tag));
    EXPECT_TRUE(PopConsole(BuildExpectedResult(tag, "registered in SW")));
  }

  EXPECT_TRUE(GetTagsFromServiceWorker(registered_tags));
}

// Verify that GetRegistration works in a service worker
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, HasTagFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);

  EXPECT_TRUE(RegisterFromServiceWorker("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(HasTagFromServiceWorker("foo_sw"));
}

// Verify that a background sync registration is deleted when site data is
// cleared.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       SyncRegistrationDeletedWhenClearingSiteData) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  // Prevent firing by going offline.
  background_sync_test_util::SetOnline(web_contents(), false);
  EXPECT_TRUE(Register("foo"));
  EXPECT_TRUE(HasTag("foo"));
  EXPECT_TRUE(RegistrationPending("foo"));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  EXPECT_FALSE(HasTag("foo"));
}

// Verify that a background sync registration, from a service worker, is deleted
// when site data is cleared.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       SyncRegistrationFromSWDeletedWhenClearingSiteData) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);

  EXPECT_TRUE(RegisterFromServiceWorker("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(HasTagFromServiceWorker("foo_sw"));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  EXPECT_FALSE(HasTagFromServiceWorker("foo"));
}

// Verify that multiple background sync registrations are deleted when site
// data is cleared.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       SyncRegistrationsDeletedWhenClearingSiteData) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo");
  registered_tags.push_back("bar");

  for (const std::string& tag : registered_tags)
    EXPECT_TRUE(Register(tag));

  EXPECT_TRUE(GetTags(registered_tags));

  for (const std::string& tag : registered_tags)
    EXPECT_TRUE(RegistrationPending(tag));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  for (const std::string& tag : registered_tags)
    EXPECT_FALSE(HasTag(tag));
}

// Verify that a sync event that is currently firing is deleted when site
// data is cleared.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       FiringSyncEventDeletedWhenClearingSiteData) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(Register("delay"));

  // Verify that it is firing.
  EXPECT_TRUE(HasTag("delay"));
  EXPECT_FALSE(RegistrationPending("delay"));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  // Verify that it was deleted.
  EXPECT_FALSE(HasTag("delay"));
}

// Disabled due to flakiness. See https://crbug.com/578952.
IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest, DISABLED_VerifyRetry) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  SetMaxSyncAttempts(2);

  EXPECT_TRUE(Register("delay"));
  EXPECT_TRUE(RejectDelayedSyncEvent());
  EXPECT_TRUE(PopConsole("ok - delay rejected"));

  // Verify that the registration is still around and waiting to try again.
  EXPECT_TRUE(RegistrationPending("delay"));
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       RegisterFromIFrameWithMainFrameHost) {
  std::string script_result;
  GURL url = https_server()->GetURL(kEmptyURL);
  EXPECT_TRUE(RunScript(BuildScriptString("registerFromLocalFrame", url.spec()),
                        &script_result));
  EXPECT_EQ(BuildExpectedResult("iframe", "registered sync"), script_result);
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       RegisterFromIFrameWithoutMainFrameHost) {
  std::string script_result;
  EXPECT_TRUE(
      RegisterFromCrossOriginFrame(kRegisterSyncFromIFrameURL, &script_result));
  EXPECT_EQ(BuildExpectedResult("frame", "failed to register sync"),
            script_result);
}

IN_PROC_BROWSER_TEST_F(BackgroundSyncBrowserTest,
                       RegisterFromServiceWorkerWithoutMainFrameHost) {
  std::string script_result;
  EXPECT_TRUE(
      RegisterFromCrossOriginFrame(kRegisterSyncFromSWURL, &script_result));
  EXPECT_EQ(BuildExpectedResult("frame", "failed to register sync"),
            script_result);
}

}  // namespace content
