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
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
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

// Tests that an attach to a tab target does not load an unloaded tab, and that
// the page of the tab is auto-attached when the tab starts to load.
class TabTargetNoLoadOnAttachBrowserTest : public TabTargetUnloadedBrowserTest {
 protected:
  // Attaches to the tab target `target_id` from the browser target, and
  // returns the session ID.
  std::string AttachToTab(const std::string& target_id) {
    base::DictValue params;
    params.Set("targetId", target_id);
    params.Set("flatten", true);
    const base::DictValue* result =
        SendCommandSync("Target.attachToTarget", std::move(params));
    if (!result || !result->FindString("sessionId")) {
      ADD_FAILURE() << "Target.attachToTarget failed for " << target_id;
      return std::string();
    }
    return *result->FindString("sessionId");
  }

  void SetAutoAttach(const std::string& session_id,
                     bool wait_for_debugger_on_start) {
    base::DictValue params;
    params.Set("autoAttach", true);
    params.Set("waitForDebuggerOnStart", wait_for_debugger_on_start);
    params.Set("flatten", true);
    ASSERT_TRUE(SendSessionCommand("Target.setAutoAttach", std::move(params),
                                   session_id, true));
  }

  static bool IsPageAttached(const base::DictValue& params) {
    const std::string* type = params.FindStringByDottedPath("targetInfo.type");
    return type && *type == "page";
  }

  // Waits for Target.attachedToTarget for a page target, and returns its
  // params.
  base::DictValue WaitForPageAttached() {
    return WaitForMatchingNotification("Target.attachedToTarget",
                                       base::BindRepeating(&IsPageAttached));
  }

  // Returns true if the client has received Target.attachedToTarget for a page
  // target after the last ClearNotifications().
  bool HasPageAttached() {
    return HasExistingNotificationMatching(
        [](const base::DictValue& notification) {
          const std::string* method = notification.FindString("method");
          const base::DictValue* params = notification.FindDict("params");
          return method && *method == "Target.attachedToTarget" && params &&
                 IsPageAttached(*params);
        });
  }

  void WaitForDetached(const std::string& session_id) {
    WaitForMatchingNotification(
        "Target.detachedFromTarget",
        base::BindLambdaForTesting([&](const base::DictValue& params) {
          const std::string* id = params.FindString("sessionId");
          return id && *id == session_id;
        }));
  }

  void RunIfWaitingForDebugger(const std::string& session_id) {
    ASSERT_TRUE(SendSessionCommand("Runtime.runIfWaitingForDebugger",
                                   base::DictValue(), session_id, true));
  }

  std::unique_ptr<WebContents> CreateRestoredNotLoadedWebContents() {
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
    return web_contents;
  }
};

// A discard detaches the page target. An attach to the unloaded tab does not
// load it, and does not auto-attach a page. A reload attaches the page again.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       DiscardDetachesPageAndAttachDoesNotLoad) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents);
  const std::string tab_session_id = AttachToTab(target_id);
  SetAutoAttach(tab_session_id, /*wait_for_debugger_on_start=*/false);
  const std::string page_session_id =
      *WaitForPageAttached().FindString("sessionId");

  ClearNotifications();
  DiscardInPlace(web_contents);
  WaitForDetached(page_session_id);

  ClearNotifications();
  const std::string tab_session_id2 = AttachToTab(target_id);
  SetAutoAttach(tab_session_id2, /*wait_for_debugger_on_start=*/false);
  // Target.getTargetInfo makes sure that the client received all events.
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  EXPECT_FALSE(HasPageAttached());
  EXPECT_FALSE(web_contents->HasUncommittedNavigationInPrimaryMainFrame());

  ClearNotifications();
  ReloadAndWait(web_contents);
  WaitForPageAttached();
  WaitForSubtypeChanged(target_id, "");
}

// A client can pause the load of an unloaded tab with waitForDebuggerOnStart.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       WaitForDebuggerOnStartPausesLoad) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  DiscardInPlace(web_contents);
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents);
  const std::string tab_session_id = AttachToTab(target_id);
  ClearNotifications();
  SetAutoAttach(tab_session_id, /*wait_for_debugger_on_start=*/true);
  EXPECT_FALSE(HasPageAttached());

  TestNavigationObserver observer(web_contents, 1);
  web_contents->GetController().Reload(ReloadType::NORMAL,
                                       /*check_for_repost=*/false);
  base::DictValue attached = WaitForPageAttached();
  EXPECT_EQ(true, attached.FindBool("waitingForDebugger"));
  const std::string page_session_id = *attached.FindString("sessionId");

  // A round trip makes sure that the browser handled the attach. The
  // navigation must not start its request before the client resumes it.
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  NavigationRequest* request = static_cast<WebContentsImpl*>(web_contents)
                                   ->GetPrimaryFrameTree()
                                   .root()
                                   ->navigation_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());

  RunIfWaitingForDebugger(page_session_id);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  WaitForSubtypeChanged(target_id, "");
}

// Target.autoAttachRelated with waitForDebuggerOnStart for the browser target
// and for an unloaded tab target installs only one navigation throttle for
// each waiting session, so that the load does not hang.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       AutoAttachRelatedDoesNotHangLoad) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  DiscardInPlace(web_contents);
  AttachAndDiscoverTabs();
  const base::DictValue* browser_info = SendCommandSync("Target.getTargetInfo");
  ASSERT_TRUE(browser_info);
  const std::string browser_target_id =
      *browser_info->FindStringByDottedPath("targetInfo.targetId");
  const std::string target_id = GetTargetId(web_contents);

  base::DictValue filter_entry;
  filter_entry.Set("type", "tab");
  base::ListValue filter;
  filter.Append(std::move(filter_entry));
  base::DictValue browser_params;
  browser_params.Set("targetId", browser_target_id);
  browser_params.Set("waitForDebuggerOnStart", true);
  browser_params.Set("filter", std::move(filter));
  ASSERT_TRUE(
      SendCommandSync("Target.autoAttachRelated", std::move(browser_params)));
  base::DictValue tab_params;
  tab_params.Set("targetId", target_id);
  tab_params.Set("waitForDebuggerOnStart", true);
  ASSERT_TRUE(
      SendCommandSync("Target.autoAttachRelated", std::move(tab_params)));

  ClearNotifications();
  TestNavigationObserver observer(web_contents, 1);
  web_contents->GetController().Reload(ReloadType::NORMAL,
                                       /*check_for_repost=*/false);
  base::DictValue attached = WaitForPageAttached();
  EXPECT_EQ(true, attached.FindBool("waitingForDebugger"));
  RunIfWaitingForDebugger(*attached.FindString("sessionId"));
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

// An attach to a tab that session restore did not load does not load it.
// Target.activateTarget loads the tab.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       ActivateTargetLoadsRestoredTab) {
  std::unique_ptr<WebContents> web_contents =
      CreateRestoredNotLoadedWebContents();
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents.get());
  const std::string tab_session_id = AttachToTab(target_id);
  ClearNotifications();
  SetAutoAttach(tab_session_id, /*wait_for_debugger_on_start=*/false);
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  EXPECT_FALSE(HasPageAttached());
  EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_FALSE(web_contents->HasUncommittedNavigationInPrimaryMainFrame());

  TestNavigationObserver observer(web_contents.get(), 1);
  base::DictValue params;
  params.Set("targetId", target_id);
  ASSERT_TRUE(SendCommandSync("Target.activateTarget", std::move(params)));
  WaitForPageAttached();
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  WaitForSubtypeChanged(target_id, "");
}

// A client can pause the load of a restored tab that has no live renderer with
// waitForDebuggerOnStart.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       WaitForDebuggerOnStartPausesRestoredTab) {
  std::unique_ptr<WebContents> web_contents =
      CreateRestoredNotLoadedWebContents();
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents.get());
  const std::string tab_session_id = AttachToTab(target_id);
  ClearNotifications();
  SetAutoAttach(tab_session_id, /*wait_for_debugger_on_start=*/true);
  EXPECT_FALSE(HasPageAttached());
  EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  TestNavigationObserver observer(web_contents.get(), 1);
  base::DictValue params;
  params.Set("targetId", target_id);
  ASSERT_TRUE(SendCommandSync("Target.activateTarget", std::move(params)));
  base::DictValue attached = WaitForPageAttached();
  EXPECT_EQ(true, attached.FindBool("waitingForDebugger"));
  const std::string page_session_id = *attached.FindString("sessionId");

  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  // Enabling Inspector while the restored tab is loading must not report a
  // false crash.
  ASSERT_TRUE(SendSessionCommand("Inspector.enable", base::DictValue(),
                                 page_session_id, true));
  EXPECT_FALSE(HasExistingNotification("Inspector.targetCrashed"));
  NavigationRequest* request = static_cast<WebContentsImpl*>(web_contents.get())
                                   ->GetPrimaryFrameTree()
                                   .root()
                                   ->navigation_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());

  RunIfWaitingForDebugger(page_session_id);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  WaitForSubtypeChanged(target_id, "");
}

// A crashed tab is unloaded, but it keeps its page target, so that clients
// can reload it through the page target.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       CrashedTabKeepsPageTarget) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  CrashTab(web_contents);
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents);
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  const std::string tab_session_id = AttachToTab(target_id);
  SetAutoAttach(tab_session_id, /*wait_for_debugger_on_start=*/false);
  base::DictValue attached = WaitForPageAttached();
  const std::string page_session_id = *attached.FindString("sessionId");
  ASSERT_TRUE(SendSessionCommand("Inspector.enable", base::DictValue(),
                                 page_session_id, true));
  EXPECT_TRUE(HasExistingNotification("Inspector.targetCrashed"));
}

// Reloading a discarded popup (a tab with an opener) must still be throttled
// by the browser when waitForDebuggerOnStart is enabled.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       WaitForDebuggerOnStartPausesDiscardedPopup) {
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  Shell* popup = OpenPopup(shell()->web_contents(), PageURL(), "popup");
  ASSERT_TRUE(popup);
  WebContents* popup_wc = popup->web_contents();
  ASSERT_TRUE(static_cast<WebContentsImpl*>(popup_wc)
                  ->GetPrimaryFrameTree()
                  .root()
                  ->opener());

  DiscardInPlace(popup_wc);
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(popup_wc);
  const std::string tab_session_id = AttachToTab(target_id);
  ClearNotifications();
  SetAutoAttach(tab_session_id, /*wait_for_debugger_on_start=*/true);
  EXPECT_FALSE(HasPageAttached());

  TestNavigationObserver observer(popup_wc, 1);
  popup_wc->GetController().Reload(ReloadType::NORMAL,
                                   /*check_for_repost=*/false);
  base::DictValue attached = WaitForPageAttached();
  EXPECT_EQ(true, attached.FindBool("waitingForDebugger"));
  const std::string page_session_id = *attached.FindString("sessionId");

  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  NavigationRequest* request = static_cast<WebContentsImpl*>(popup_wc)
                                   ->GetPrimaryFrameTree()
                                   .root()
                                   ->navigation_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());

  RunIfWaitingForDebugger(page_session_id);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  WaitForSubtypeChanged(target_id, "");
}

// If a load of an unloaded tab stops before committing (e.g. a 204 response
// with a speculative RFH), the page target must detach again so that a later
// reload attaches as a new target and pauses for waitForDebuggerOnStart.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       NonCommittingLoadDetachesPageAndNextReloadPauses) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  // Keep the renderer process alive via a popup so an in-place discard leaves
  // a live current frame and a cross-site navigation creates a speculative RFH.
  Shell* popup = OpenPopup(web_contents, PageURL(), "popup");
  ASSERT_TRUE(popup);
  DiscardInPlace(web_contents);

  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents);
  const std::string tab_session_id = AttachToTab(target_id);
  ClearNotifications();
  SetAutoAttach(tab_session_id, /*wait_for_debugger_on_start=*/true);
  EXPECT_FALSE(HasPageAttached());

  // Start a cross-site 204 navigation that does not commit.
  {
    TestNavigationObserver nocontent_observer(web_contents, 1);
    web_contents->GetController().LoadURL(
        embedded_test_server()->GetURL("b.test", "/nocontent"), Referrer(),
        ui::PAGE_TRANSITION_TYPED, std::string());
    base::DictValue attached = WaitForPageAttached();
    EXPECT_EQ(true, attached.FindBool("waitingForDebugger"));
    const std::string page_session_id = *attached.FindString("sessionId");
    // Do not wait for a response: the 204 navigation can finish and detach
    // `page_session_id` before the renderer replies.
    SendSessionCommand("Runtime.runIfWaitingForDebugger", base::DictValue(),
                       page_session_id, /*wait=*/false);
    nocontent_observer.Wait();
    WaitForDetached(page_session_id);
  }
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));

  // A subsequent reload must attach the page again and pause before starting
  // the request.
  ClearNotifications();
  TestNavigationObserver reload_observer(web_contents, 1);
  web_contents->GetController().Reload(ReloadType::NORMAL,
                                       /*check_for_repost=*/false);
  base::DictValue attached = WaitForPageAttached();
  EXPECT_EQ(true, attached.FindBool("waitingForDebugger"));
  const std::string page_session_id = *attached.FindString("sessionId");
  EXPECT_EQ(kUnloaded, GetSubtype(target_id));
  NavigationRequest* request = static_cast<WebContentsImpl*>(web_contents)
                                   ->GetPrimaryFrameTree()
                                   .root()
                                   ->navigation_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());

  RunIfWaitingForDebugger(page_session_id);
  reload_observer.Wait();
  EXPECT_TRUE(reload_observer.last_navigation_succeeded());
  WaitForSubtypeChanged(target_id, "");
}

// Detaching the paused page session (via Target.detachFromTarget or
// Target.setAutoAttach(false)) while a load is paused at WILL_START_REQUEST
// must resume the navigation cleanly.
IN_PROC_BROWSER_TEST_F(TabTargetNoLoadOnAttachBrowserTest,
                       DetachWhileLoadPausedResumesNavigation) {
  WebContents* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(shell(), PageURL()));
  AttachAndDiscoverTabs();
  const std::string target_id = GetTargetId(web_contents);
  const std::string tab_session_id = AttachToTab(target_id);

  for (bool disable_auto_attach : {false, true}) {
    DiscardInPlace(web_contents);
    ClearNotifications();
    SetAutoAttach(tab_session_id, /*wait_for_debugger_on_start=*/true);
    EXPECT_FALSE(HasPageAttached());

    TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(ReloadType::NORMAL,
                                         /*check_for_repost=*/false);
    base::DictValue attached = WaitForPageAttached();
    EXPECT_EQ(true, attached.FindBool("waitingForDebugger"));
    const std::string page_session_id = *attached.FindString("sessionId");

    EXPECT_EQ(kUnloaded, GetSubtype(target_id));
    NavigationRequest* request = static_cast<WebContentsImpl*>(web_contents)
                                     ->GetPrimaryFrameTree()
                                     .root()
                                     ->navigation_request();
    ASSERT_TRUE(request);
    EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());

    if (disable_auto_attach) {
      base::DictValue params;
      params.Set("autoAttach", false);
      params.Set("waitForDebuggerOnStart", false);
      params.Set("flatten", true);
      ASSERT_TRUE(SendSessionCommand("Target.setAutoAttach", std::move(params),
                                     tab_session_id, true));
    } else {
      base::DictValue params;
      params.Set("sessionId", page_session_id);
      ASSERT_TRUE(SendSessionCommand("Target.detachFromTarget",
                                     std::move(params), tab_session_id, true));
    }

    WaitForDetached(page_session_id);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    WaitForSubtypeChanged(target_id, "");
  }
}

}  // namespace content
