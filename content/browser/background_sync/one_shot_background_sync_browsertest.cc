// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_base_browsertest.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/browser_test.h"
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
  OneShotBackgroundSyncBrowserTest() = default;

  OneShotBackgroundSyncBrowserTest(const OneShotBackgroundSyncBrowserTest&) =
      delete;
  OneShotBackgroundSyncBrowserTest& operator=(
      const OneShotBackgroundSyncBrowserTest&) = delete;

  ~OneShotBackgroundSyncBrowserTest() override = default;

  void Register(const std::string& tag);
  void RegisterFromServiceWorker(const std::string& tag);
  EvalJsResult RegisterFromCrossOriginFrame(const std::string& frame_url);
  void WaitForTagRemoval(const std::string& tag, int64_t pauses_ms = 5);
  bool HasTag(const std::string& tag);
  bool HasTagFromServiceWorker(const std::string& tag);
  bool MatchTags(const std::string& script_result,
                 const std::vector<std::string>& expected_tags);
  bool GetTags(const std::vector<std::string>& expected_tags);
  bool GetTagsFromServiceWorker(const std::vector<std::string>& expected_tags);
  void RejectDelayedSyncEvent();
};

void OneShotBackgroundSyncBrowserTest::Register(const std::string& tag) {
  ASSERT_EQ(
      BuildExpectedResult(tag, "registered"),
      EvalJs(web_contents(), BuildScriptString("registerOneShotSync", tag)));
}

void OneShotBackgroundSyncBrowserTest::RegisterFromServiceWorker(
    const std::string& tag) {
  ASSERT_EQ(
      BuildExpectedResult(tag, "register sent to SW"),
      EvalJs(web_contents(),
             BuildScriptString("registerOneShotSyncFromServiceWorker", tag)));
}

EvalJsResult OneShotBackgroundSyncBrowserTest::RegisterFromCrossOriginFrame(
    const std::string& frame_url) {
  // Start a second https server to use as a second origin.
  net::EmbeddedTestServer alt_server(net::EmbeddedTestServer::TYPE_HTTPS);
  alt_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  EXPECT_TRUE(alt_server.Start());

  GURL url = alt_server.GetURL(frame_url);
  return EvalJs(
      web_contents(),
      BuildScriptString("registerOneShotSyncFromCrossOriginFrame", url.spec()));
}

void OneShotBackgroundSyncBrowserTest::WaitForTagRemoval(const std::string& tag,
                                                         int64_t pauses_ms) {
  while (HasTag(tag)) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(pauses_ms));
    run_loop.Run();
  }
}

bool OneShotBackgroundSyncBrowserTest::HasTag(const std::string& tag) {
  return EvalJs(web_contents(), BuildScriptString("hasOneShotSyncTag", tag)) ==
         BuildExpectedResult(tag, "found");
}

bool OneShotBackgroundSyncBrowserTest::HasTagFromServiceWorker(
    const std::string& tag) {
  EXPECT_EQ(
      "ok - hasTag sent to SW",
      EvalJs(web_contents(),
             BuildScriptString("hasOneShotSyncTagFromServiceWorker", tag)));

  return PopConsoleString() == BuildExpectedResult(tag, "found");
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
  std::string script_result =
      EvalJs(web_contents(), "getOneShotSyncTags()").ExtractString();

  return MatchTags(script_result, expected_tags);
}

bool OneShotBackgroundSyncBrowserTest::GetTagsFromServiceWorker(
    const std::vector<std::string>& expected_tags) {
  EXPECT_EQ("ok - getTags sent to SW",
            EvalJs(web_contents(), "getOneShotSyncTagsFromServiceWorker()"));

  return MatchTags(PopConsoleString().ExtractString(), expected_tags);
}

void OneShotBackgroundSyncBrowserTest::RejectDelayedSyncEvent() {
  ASSERT_EQ(BuildExpectedResult("delay", "rejecting"),
            EvalJs(web_contents(), "rejectDelayedSyncEvent()"));
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromControlledDocument) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  Register("foo");
  EXPECT_EQ("foo fired", PopConsoleString());
  WaitForTagRemoval("foo");
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromUncontrolledDocument) {
  RegisterServiceWorker();

  Register("foo");
  EXPECT_EQ("foo fired", PopConsoleString());
  WaitForTagRemoval("foo");
}

// Verify that Register works in a service worker
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromServiceWorker) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  RegisterFromServiceWorker("foo_sw");
  EXPECT_EQ("ok - foo_sw registered in SW", PopConsoleString());
  EXPECT_EQ("foo_sw fired", PopConsoleString());
  WaitForTagRemoval("foo_sw");
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegistrationDelaysForNetwork) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  // Prevent firing by going offline.
  background_sync_test_util::SetOnline(web_contents(), false);
  Register("foo");
  EXPECT_TRUE(HasTag("foo"));
  EXPECT_TRUE(RegistrationPending("foo"));

  // Resume firing by going online.
  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_EQ("foo fired", PopConsoleString());
  WaitForTagRemoval("foo");
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest, WaitUntil) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  Register("delay");

  // Verify that it is firing.
  EXPECT_TRUE(HasTag("delay"));
  EXPECT_FALSE(RegistrationPending("delay"));

  // Complete the task.
  CompleteDelayedSyncEvent();
  EXPECT_EQ("ok - delay completed", PopConsoleString());

  // Verify that it finished firing.
  WaitForTagRemoval("delay");
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest, WaitUntilReject) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  Register("delay");

  // Verify that it is firing.
  EXPECT_TRUE(HasTag("delay"));
  EXPECT_FALSE(RegistrationPending("delay"));

  // Complete the task.
  RejectDelayedSyncEvent();
  EXPECT_EQ("ok - delay rejected", PopConsoleString());
  WaitForTagRemoval("delay");
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest, Incognito) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), false);
  Register("normal");
  EXPECT_TRUE(RegistrationPending("normal"));

  // Go incognito and verify that incognito doesn't see the registration.
  SetIncognitoMode(true);

  // Tell the new network observer that we're offline (it initializes from
  // NetworkChangeNotifier::GetCurrentConnectionType() which is not mocked out
  // in this test).
  background_sync_test_util::SetOnline(web_contents(), false);

  LoadTestPage(kDefaultTestURL);
  RegisterServiceWorker();

  EXPECT_FALSE(HasTag("normal"));

  Register("incognito");
  EXPECT_TRUE(RegistrationPending("incognito"));

  // Switch back and make sure the registration is still there.
  SetIncognitoMode(false);
  LoadTestPage(kDefaultTestURL);  // Should be controlled.

  EXPECT_TRUE(HasTag("normal"));
  EXPECT_FALSE(HasTag("incognito"));
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest, GetTags) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo");
  registered_tags.push_back("bar");

  for (const std::string& tag : registered_tags)
    Register(tag);

  EXPECT_TRUE(GetTags(registered_tags));
}

// Verify that GetOneShotSyncRegistrations works in a service worker
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       GetRegistrationsFromServiceWorker) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo_sw");
  registered_tags.push_back("bar_sw");

  for (const std::string& tag : registered_tags) {
    RegisterFromServiceWorker(tag);
    EXPECT_EQ(BuildExpectedResult(tag, "registered in SW"), PopConsoleString());
  }

  EXPECT_TRUE(GetTagsFromServiceWorker(registered_tags));
}

// Verify that GetOneShotSyncRegistration works in a service worker
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       HasTagFromServiceWorker) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);

  RegisterFromServiceWorker("foo_sw");
  EXPECT_EQ("ok - foo_sw registered in SW", PopConsoleString());
  EXPECT_TRUE(HasTagFromServiceWorker("foo_sw"));
}

// Verify that a background sync registration is deleted when site data is
// cleared.
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       SyncRegistrationDeletedWhenClearingSiteData) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  // Prevent firing by going offline.
  background_sync_test_util::SetOnline(web_contents(), false);
  Register("foo");
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
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);

  RegisterFromServiceWorker("foo_sw");
  EXPECT_EQ("ok - foo_sw registered in SW", PopConsoleString());
  EXPECT_TRUE(HasTagFromServiceWorker("foo_sw"));

  // Simulate a user clearing site data (including Service Workers, crucially),
  // by clearing data from the storage partition.
  ClearStoragePartitionData();

  // Use HasTag() instead of HasTagServiceWorker() because clearing site data
  // immediately terminates the service worker when removing it from the
  // registration.
  EXPECT_FALSE(HasTag("foo"));
}

// Verify that multiple background sync registrations are deleted when site
// data is cleared.
IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       SyncRegistrationsDeletedWhenClearingSiteData) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  std::vector<std::string> registered_tags;
  EXPECT_TRUE(GetTags(registered_tags));

  background_sync_test_util::SetOnline(web_contents(), false);
  registered_tags.push_back("foo");
  registered_tags.push_back("bar");

  for (const std::string& tag : registered_tags)
    Register(tag);

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
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  background_sync_test_util::SetOnline(web_contents(), true);
  Register("delay");

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
  GURL url = https_server()->GetURL(kEmptyURL);
  EXPECT_EQ(BuildExpectedResult("iframe", "registered sync"),
            EvalJs(web_contents(),
                   BuildScriptString("registerOneShotSyncFromLocalFrame",
                                     url.spec())));
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromIFrameWithoutMainFrameHost) {
  EXPECT_EQ(BuildExpectedResult("frame", "failed to register sync"),
            RegisterFromCrossOriginFrame(kRegisterSyncFromIFrameURL));
}

IN_PROC_BROWSER_TEST_F(OneShotBackgroundSyncBrowserTest,
                       RegisterFromServiceWorkerWithoutMainFrameHost) {
  EXPECT_EQ(BuildExpectedResult("frame", "failed to register sync"),
            RegisterFromCrossOriginFrame(kRegisterSyncFromSWURL));
}

}  // namespace content
