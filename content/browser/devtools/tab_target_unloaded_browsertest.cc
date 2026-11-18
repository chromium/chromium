// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

constexpr char kUnloaded[] = "unloaded";

// Starts a navigation to `url` and waits until it finishes. The navigation
// does not have to commit.
void NavigateAndWaitForFinish(WebContents* web_contents, const GURL& url) {
  TestNavigationObserver observer(web_contents, 1);
  web_contents->GetController().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  observer.Wait();
}

void ReloadAndWait(WebContents* web_contents) {
  TestNavigationObserver observer(web_contents, 1);
  web_contents->GetController().Reload(ReloadType::NORMAL,
                                       /*check_for_repost=*/false);
  observer.Wait();
}

void DiscardInPlace(WebContents* web_contents) {
  base::RunLoop run_loop;
  web_contents->Discard(run_loop.QuitClosure());
  run_loop.Run();
}

std::string GetTargetId(WebContents* web_contents) {
  return DevToolsAgentHost::GetOrCreateForTab(web_contents)->GetId();
}

}  // namespace

// Tests the "unloaded" subtype of tab targets for the different ways in which
// a tab can lose its document, and that the subtype is cleared when a new
// document commits.
class TabTargetUnloadedBrowserTest : public DevToolsProtocolTest {
 public:
  TabTargetUnloadedBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kWebContentsDiscard);
  }

  void SetUpOnMainThread() override {
    DevToolsProtocolTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  GURL PageURL() { return embedded_test_server()->GetURL("/title1.html"); }
  GURL NoContentURL() { return embedded_test_server()->GetURL("/nocontent"); }

  // Attaches to the browser target and discovers tab targets, so that the
  // client receives Target.targetInfoChanged for tabs.
  void AttachAndDiscoverTabs() {
    AttachToBrowserTarget();
    DiscoverTabs();
  }

  // Discovers tab targets from the browser target.
  void DiscoverTabs() {
    base::DictValue filter_entry;
    filter_entry.Set("type", "tab");
    base::ListValue filter;
    filter.Append(std::move(filter_entry));
    base::DictValue params;
    params.Set("discover", true);
    params.Set("filter", std::move(filter));
    ASSERT_TRUE(
        SendCommandSync("Target.setDiscoverTargets", std::move(params)));
  }

  // Auto-attaches to tab targets from the browser target.
  void AutoAttachToTabs() {
    base::DictValue filter_entry;
    filter_entry.Set("type", "tab");
    base::ListValue filter;
    filter.Append(std::move(filter_entry));
    base::DictValue params;
    params.Set("autoAttach", true);
    params.Set("waitForDebuggerOnStart", false);
    params.Set("flatten", true);
    params.Set("filter", std::move(filter));
    ASSERT_TRUE(SendCommandSync("Target.setAutoAttach", std::move(params)));
  }

  // Returns the subtype from Target.getTargetInfo, or an empty string if the
  // target info has no subtype.
  std::string GetSubtype(const std::string& target_id) {
    base::DictValue params;
    params.Set("targetId", target_id);
    const base::DictValue* result =
        SendCommandSync("Target.getTargetInfo", std::move(params));
    if (!result) {
      ADD_FAILURE() << "Target.getTargetInfo failed for " << target_id;
      return std::string();
    }
    const std::string* subtype =
        result->FindStringByDottedPath("targetInfo.subtype");
    return subtype ? *subtype : std::string();
  }

  // Waits for Target.targetInfoChanged for `target_id` with `subtype`. An empty
  // `subtype` matches target info without a subtype.
  void WaitForSubtypeChanged(const std::string& target_id,
                             const std::string& subtype) {
    WaitForMatchingNotification(
        "Target.targetInfoChanged",
        base::BindRepeating(
            [](const std::string& target_id, const std::string& subtype,
               const base::DictValue& params) {
              const std::string* id =
                  params.FindStringByDottedPath("targetInfo.targetId");
              const std::string* actual =
                  params.FindStringByDottedPath("targetInfo.subtype");
              return id && *id == target_id &&
                     (actual ? *actual : std::string()) == subtype;
            },
            target_id, subtype));
  }

  // Returns true if the client has received Target.targetInfoChanged for
  // `target_id` after the last ClearNotifications().
  bool HasTargetInfoChanged(const std::string& target_id) {
    return HasExistingNotificationMatching(
        [&target_id](const base::DictValue& notification) {
          const std::string* method = notification.FindString("method");
          const std::string* id =
              notification.FindStringByDottedPath("params.targetInfo.targetId");
          return method && *method == "Target.targetInfoChanged" && id &&
                 *id == target_id;
        });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest, InPlaceDiscard) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents);
  EXPECT_EQ("", GetSubtype(target_id));

  ClearNotifications();
  DiscardInPlace(web_contents);
  WaitForSubtypeChanged(target_id, kUnloaded);
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));

  // A second discard of the unloaded tab does not send
  // Target.targetInfoChanged. WebContents::Discard() does not call its
  // callback for a tab that is already discarded, so do not wait for it.
  ClearNotifications();
  web_contents->Discard(base::NullCallback());
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  EXPECT_FALSE(HasTargetInfoChanged(target_id));

  // A navigation that does not commit keeps the subtype, and does not send
  // Target.targetInfoChanged.
  ClearNotifications();
  NavigateAndWaitForFinish(web_contents, NoContentURL());
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  EXPECT_FALSE(HasTargetInfoChanged(target_id));

  ClearNotifications();
  ReloadAndWait(web_contents);
  WaitForSubtypeChanged(target_id, "");
  EXPECT_EQ("", GetSubtype(target_id));
}

// If the renderer process is shared with a different tab, the process stays
// alive after an in-place discard, and the frame keeps its committed state.
IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest,
                       InPlaceDiscardInSharedProcess) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  // A popup in the same BrowsingInstance and site uses the same process.
  Shell* popup = OpenPopup(web_contents, PageURL(), "popup");
  ASSERT_TRUE(popup);
  ASSERT_EQ(web_contents->GetPrimaryMainFrame()->GetProcess(),
            popup->web_contents()->GetPrimaryMainFrame()->GetProcess());
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents);

  ClearNotifications();
  DiscardInPlace(web_contents);
  WaitForSubtypeChanged(target_id, kUnloaded);
  EXPECT_TRUE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  NavigateAndWaitForFinish(web_contents, NoContentURL());
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));

  ClearNotifications();
  ReloadAndWait(web_contents);
  WaitForSubtypeChanged(target_id, "");
}

// A host that is created after a navigation started on a discarded tab
// reports the subtype. WebContents::WasDiscarded() is false at that time.
IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest,
                       HostCreatedAfterDiscardAndNavigation) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  ASSERT_FALSE(DevToolsAgentHost::GetForTab(web_contents));
  DiscardInPlace(web_contents);
  NavigateAndWaitForFinish(web_contents, NoContentURL());
  ASSERT_FALSE(web_contents->WasDiscarded());

  AttachAndDiscoverTabs();
  EXPECT_EQ(kUnloaded, GetSubtype(GetTargetId(web_contents)));
}

// Does the same steps as session restore for a tab that is not loaded.
IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest, RestoredNotLoaded) {
  BrowserContext* browser_context =
      shell()->web_contents()->GetBrowserContext();
  WebContents::CreateParams create_params(browser_context);
  create_params.initially_hidden = true;
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(create_params);
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(NavigationController::CreateNavigationEntry(
      PageURL(), Referrer(), /*initiator_origin=*/std::nullopt,
      /*initiator_base_url=*/std::nullopt, ui::PAGE_TRANSITION_RELOAD,
      /*is_renderer_initiated=*/false, std::string(), browser_context,
      /*blob_url_loader_factory=*/nullptr));
  web_contents->GetController().Restore(0, RestoreType::kRestored, &entries);

  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents.get());
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));

  NavigateAndWaitForFinish(web_contents.get(), NoContentURL());
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));

  ClearNotifications();
  {
    TestNavigationObserver observer(web_contents.get(), 1);
    web_contents->GetController().SetNeedsReload();
    web_contents->GetController().LoadIfNecessary();
    observer.Wait();
  }
  WaitForSubtypeChanged(target_id, "");
}

// Without a primary frame host, a tab target reads its values from the
// WebContents, as RenderFrameDevToolsAgentHost does for a primary main frame.
IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest,
                       ValuesWithoutPrimaryFrameHost) {
  BrowserContext* browser_context =
      shell()->web_contents()->GetBrowserContext();
  WebContents::CreateParams create_params(browser_context);
  create_params.initially_hidden = true;
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(create_params);
  std::unique_ptr<NavigationEntry> entry =
      NavigationController::CreateNavigationEntry(
          PageURL(), Referrer(), /*initiator_origin=*/std::nullopt,
          /*initiator_base_url=*/std::nullopt, ui::PAGE_TRANSITION_RELOAD,
          /*is_renderer_initiated=*/false, std::string(), browser_context,
          /*blob_url_loader_factory=*/nullptr);
  const GURL favicon_url = embedded_test_server()->GetURL("/favicon.ico");
  entry->SetTitle(u"Restored title");
  entry->GetFavicon().url = favicon_url;
  entry->GetFavicon().valid = true;
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(entry));
  web_contents->GetController().Restore(0, RestoreType::kRestored, &entries);

  scoped_refptr<DevToolsAgentHost> host =
      DevToolsAgentHost::GetOrCreateForTab(web_contents.get());
  ASSERT_FALSE(DevToolsAgentHost::HasFor(web_contents.get()));
  EXPECT_EQ("Restored title", host->GetTitle());
  EXPECT_EQ(PageURL(), host->GetURL());
  EXPECT_EQ(favicon_url, host->GetFaviconURL());
  EXPECT_EQ(web_contents->GetLastActiveTimeTicks(),
            host->GetLastActivityTime());
}

IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest, RendererCrash) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents);

  ClearNotifications();
  CrashTab(web_contents);
  WaitForSubtypeChanged(target_id, kUnloaded);
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));

  NavigateAndWaitForFinish(web_contents, NoContentURL());
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));

  ClearNotifications();
  ReloadAndWait(web_contents);
  WaitForSubtypeChanged(target_id, "");
}

// A new tab that has not navigated has no document either, but it is not
// unloaded.
IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest, NewTabWithoutURL) {
  Shell* new_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size());
  AttachAndDiscoverTabs();
  EXPECT_EQ("", GetSubtype(GetTargetId(new_shell->web_contents())));
}

// A client that only auto-attaches to tab targets gets exactly one
// Target.targetInfoChanged for each change of the subtype.
IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest, AutoAttachOnly) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  AttachToBrowserTarget();
  AutoAttachToTabs();
  const std::string target_id = GetTargetId(web_contents);

  ClearNotifications();
  DiscardInPlace(web_contents);
  WaitForSubtypeChanged(target_id, kUnloaded);
  // Target.getTargetInfo makes sure that the client received all events.
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  EXPECT_FALSE(HasTargetInfoChanged(target_id));

  ClearNotifications();
  ReloadAndWait(web_contents);
  WaitForSubtypeChanged(target_id, "");
  EXPECT_EQ("", GetSubtype(target_id));
  EXPECT_FALSE(HasTargetInfoChanged(target_id));
}

// A client that auto-attaches to tab targets and also discovers them gets
// exactly one Target.targetInfoChanged for each change of the subtype.
IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest, AutoAttachAndDiscover) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  AttachAndDiscoverTabs();
  AutoAttachToTabs();
  const std::string target_id = GetTargetId(web_contents);

  ClearNotifications();
  DiscardInPlace(web_contents);
  WaitForSubtypeChanged(target_id, kUnloaded);
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  EXPECT_FALSE(HasTargetInfoChanged(target_id));

  ClearNotifications();
  ReloadAndWait(web_contents);
  WaitForSubtypeChanged(target_id, "");
  EXPECT_EQ("", GetSubtype(target_id));
  EXPECT_FALSE(HasTargetInfoChanged(target_id));
}

// A client that auto-attaches to tab targets before it discovers them gets
// Target.targetCreated for the existing tab, and then exactly one
// Target.targetInfoChanged for each change of the subtype.
IN_PROC_BROWSER_TEST_F(TabTargetUnloadedBrowserTest, AutoAttachThenDiscover) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  AttachToBrowserTarget();
  AutoAttachToTabs();
  const std::string target_id = GetTargetId(web_contents);

  ClearNotifications();
  DiscoverTabs();
  int target_created_count = 0;
  HasExistingNotificationMatching([&](const base::DictValue& notification) {
    const std::string* method = notification.FindString("method");
    const std::string* id =
        notification.FindStringByDottedPath("params.targetInfo.targetId");
    if (method && *method == "Target.targetCreated" && id && *id == target_id) {
      ++target_created_count;
    }
    return false;
  });
  EXPECT_EQ(1, target_created_count);

  ClearNotifications();
  DiscardInPlace(web_contents);
  WaitForSubtypeChanged(target_id, kUnloaded);
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  EXPECT_FALSE(HasTargetInfoChanged(target_id));
}

}  // namespace content
