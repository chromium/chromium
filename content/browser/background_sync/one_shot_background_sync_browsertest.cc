// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_base_browsertest.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class OneShotBackgroundSyncBrowserTest : public BackgroundSyncBaseBrowserTest {
 public:
  OneShotBackgroundSyncBrowserTest() {}
  ~OneShotBackgroundSyncBrowserTest() override {}

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
  bool RejectDelayedSyncEvent();

 private:
  DISALLOW_COPY_AND_ASSIGN(OneShotBackgroundSyncBrowserTest);
};

bool OneShotBackgroundSyncBrowserTest::Register(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("registerOneShotSync", tag), &script_result));
  return script_result == BuildExpectedResult(tag, "registered");
}

bool OneShotBackgroundSyncBrowserTest::RegisterFromServiceWorker(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("registerOneShotSyncFromServiceWorker", tag),
                &script_result));
  return script_result == BuildExpectedResult(tag, "register sent to SW");
}

bool OneShotBackgroundSyncBrowserTest::RegisterFromCrossOriginFrame(
    const std::string& frame_url,
    std::string* script_result) {
  // Start a second https server to use as a second origin.
  net::EmbeddedTestServer alt_server(net::EmbeddedTestServer::TYPE_HTTPS);
  alt_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  EXPECT_TRUE(alt_server.Start());

  GURL url = alt_server.GetURL(frame_url);
  return RunScript(
      BuildScriptString("registerOneShotSyncFromCrossOriginFrame", url.spec()),
      script_result);
}

bool OneShotBackgroundSyncBrowserTest::HasTag(const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("hasOneShotSyncTag", tag), &script_result));
  return script_result == BuildExpectedResult(tag, "found");
}

bool OneShotBackgroundSyncBrowserTest::HasTagFromServiceWorker(
    const std::string& tag) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript(BuildScriptString("hasOneShotSyncTagFromServiceWorker", tag),
                &script_result));
  EXPECT_TRUE(script_result == "ok - hasTag sent to SW");

  return PopConsole(BuildExpectedResult(tag, "found"));
}

bool OneShotBackgroundSyncBrowserTest::MatchTags(
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

bool OneShotBackgroundSyncBrowserTest::GetTags(
    const std::vector<std::string>& expected_tags) {
  std::string script_result;
  EXPECT_TRUE(RunScript("getOneShotSyncTags()", &script_result));

  return MatchTags(script_result, expected_tags);
}

bool OneShotBackgroundSyncBrowserTest::GetTagsFromServiceWorker(
    const std::vector<std::string>& expected_tags) {
  std::string script_result;
  EXPECT_TRUE(
      RunScript("getOneShotSyncTagsFromServiceWorker()", &script_result));
  EXPECT_TRUE(script_result == "ok - getTags sent to SW");

  return MatchTags(PopConsoleString(), expected_tags);
}

bool OneShotBackgroundSyncBrowserTest::RejectDelayedSyncEvent() {
  std::string script_result;
  EXPECT_TRUE(RunScript("rejectDelayedSyncEvent()", &script_result));
  return script_result == BuildExpectedResult("delay", "rejecting");
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromControlledDocument) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(Register("foo"));
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(HasTag("foo"));
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromUncontrolledDocument) {
  EXPECT_TRUE(RegisterServiceWorker());

  EXPECT_TRUE(Register("foo"));
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_FALSE(HasTag("foo"));
}

// Verify that Register works in a service worker
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(RegisterFromServiceWorker("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(PopConsole("foo_sw fired"));
  EXPECT_FALSE(HasTag("foo_sw"));
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
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

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest, WaitUntil) {
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

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest, WaitUntilReject) {
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

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest, Incognito) {
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

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest, GetTags) {
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

// Verify that GetOneShotSyncRegistrations works in a service worker
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
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

// Verify that GetOneShotSyncRegistration works in a service worker
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       HasTagFromServiceWorker) {
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
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
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
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
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
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
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
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
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

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromIFrameWithMainFrameHost) {
  std::string script_result;
  GURL url = https_server()->GetURL(kEmptyURL);
  EXPECT_TRUE(RunScript(
      BuildScriptString("registerOneShotSyncFromLocalFrame", url.spec()),
      &script_result));
  EXPECT_EQ(BuildExpectedResult("iframe", "registered sync"), script_result);
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromIFrameWithoutMainFrameHost) {
  std::string script_result;
  EXPECT_TRUE(
      RegisterFromCrossOriginFrame(kRegisterSyncFromIFrameURL, &script_result));
  EXPECT_EQ(BuildExpectedResult("frame", "failed to register sync"),
            script_result);
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromServiceWorkerWithoutMainFrameHost) {
  std::string script_result;
  EXPECT_TRUE(
      RegisterFromCrossOriginFrame(kRegisterSyncFromSWURL, &script_result));
  EXPECT_EQ(BuildExpectedResult("frame", "failed to register sync"),
            script_result);
}

}  // namespace content
