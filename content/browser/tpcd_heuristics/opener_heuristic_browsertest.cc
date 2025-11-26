// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/features.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/btm/btm_browsertest_utils.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_storage.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/browser/btm/btm_utils.h"
#include "content/browser/tpcd_heuristics/opener_heuristic_metrics.h"
#include "content/browser/tpcd_heuristics/opener_heuristic_tab_helper.h"
#include "content/browser/tpcd_heuristics/opener_heuristic_utils.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/window_open_disposition.h"

using base::test::HasValue;
using base::test::ValueIs;
using content_settings::features::EnableForIframeTypes;
using testing::ElementsAre;
using testing::Field;
using testing::Optional;
using testing::Pair;

namespace content {

namespace {

struct AccessGrantTestCase {
  bool write_grant_enabled = false;
  bool disable_for_ad_tagged_popups = false;
};

[[maybe_unused]] const AccessGrantTestCase kAccessGrantTestCases[] = {
    {.write_grant_enabled = false, .disable_for_ad_tagged_popups = false},
    {.write_grant_enabled = true, .disable_for_ad_tagged_popups = false},
    {.write_grant_enabled = true, .disable_for_ad_tagged_popups = true}};

struct InteractionTimesTestCase {
  std::string hours_since_activation_time;
  std::string hours_since_authentication_time;
  std::string expected_hours_since_interaction;
};

// "-1" hours since interaction represents no past interaction
const InteractionTimesTestCase InteractionTimeTestCases[] = {
    {"5", "2", "2"},
    {"2", "5", "2"},
    {"-1", "2", "2"},
    {"5", "-1", "5"},
    {"-1", "-1", "-1"}};

enum class InteractionTypeTestCase {
  USER_ACTIVATION = 0,
  WEB_AUTHENTICATION = 1
};

const InteractionTypeTestCase InteractionTypeTestCases[] = {
    InteractionTypeTestCase::USER_ACTIVATION,
    InteractionTypeTestCase::WEB_AUTHENTICATION};

// Waits for a pop-up to open.
class PopupObserver : public WebContentsObserver {
 public:
  explicit PopupObserver(
      WebContents* web_contents,
      WindowOpenDisposition open_disposition = WindowOpenDisposition::NEW_POPUP)
      : WebContentsObserver(web_contents),
        open_disposition_(open_disposition) {}

  void Wait() { run_loop_.Run(); }
  WebContents* popup() { return popup_; }

 private:
  // WebContentsObserver overrides:
  void DidOpenRequestedURL(WebContents* new_contents,
                           RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
    if (!popup_ && disposition == open_disposition_) {
      popup_ = new_contents;
      run_loop_.Quit();
    }
  }

  const WindowOpenDisposition open_disposition_;
  raw_ptr<WebContents> popup_ = nullptr;
  base::RunLoop run_loop_;
};

}  // namespace

// SubresourceFilterBrowserTest is necessary to test ad-tagging related
// behaviors.
class OpenerHeuristicBrowserTest : public ContentBrowserTest,
                                   public TestDevToolsProtocolClient {
 public:
  OpenerHeuristicBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // We host the "images" on an HTTPS server, because for it to write a
    // cookie, the cookie needs to be SameSite=None and Secure.
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
  }

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kTpcdHeuristicsGrants,
          tpcd_heuristics_grants_params_},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsHeuristics", "true"}}}},
        {});

    OpenerHeuristicTabHelper::SetClockForTesting(&clock_);
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");

    // TODO: crbug.com/376625002 - disabled for the move to //content since
    // SubresourceFilterBrowserTest is unavailable. Either find a way to
    // implement this test in //content or move it back to //chrome.
    //
    // // These rules apply an ad-tagging param to scripts in ad_script.js,
    // // and to cookies marked with the `isad=1` param value.
    // SetRulesetWithRules(
    //     {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
    //      subresource_filter::testing::CreateSuffixRule("isad=1")});

    BtmServiceImpl::Get(GetActiveWebContents()->GetBrowserContext())
        ->SetStorageClockForTesting(&clock_);

    ukm::InitializeSourceUrlRecorderForWebContents(GetActiveWebContents());

    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), GURL("about:blank")));
    // Open and reset DevTools.
    AttachToWebContents(GetActiveWebContents());
    SendCommandSync("Audits.enable");
    ClearNotifications();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server_.ShutdownAndWaitUntilComplete());

    DetachProtocolClient();
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

  OpenerHeuristicTabHelper* GetTabHelper() {
    return OpenerHeuristicTabHelper::FromWebContents(GetActiveWebContents());
  }

  BtmServiceImpl* GetBtmService() {
    return BtmServiceImpl::Get(GetActiveWebContents()->GetBrowserContext());
  }

  void RecordUserActivationInteraction(const GURL& url, base::Time time) {
    auto* btm = GetBtmService();
    btm->storage()
        ->AsyncCall(&BtmStorage::RecordUserActivation)
        .WithArgs(url, time);
    btm->storage()->FlushPostedTasksForTesting();
  }

  void RecordAuthenticationInteraction(const GURL& url, base::Time time) {
    auto* btm = GetBtmService();
    btm->storage()
        ->AsyncCall(&BtmStorage::RecordWebAuthnAssertion)
        .WithArgs(url, time);
    btm->storage()->FlushPostedTasksForTesting();
  }

  // Open a popup window, navigate it to `url`, and return its WebContents.
  base::expected<WebContents*, std::string> OpenPopup(const GURL& url) {
    return OpenPopup(url, url);
  }

  // Open a popup window, start a navigation to `initial_url`, confirm that it
  // lands on `final_url`, and return its WebContents.
  base::expected<WebContents*, std::string> OpenPopup(const GURL& initial_url,
                                                      const GURL& final_url) {
    auto* web_contents = GetActiveWebContents();
    if (web_contents->GetLastCommittedURL().is_empty()) {
      // We can't call window.open() if we're not on a page. Go to about:blank.
      if (!NavigateToURL(web_contents, GURL("about:blank"))) {
        return base::unexpected("failed to navigate to about:blank");
      }
    }
    PopupObserver observer(web_contents);
    if (!ExecJs(web_contents,
                JsReplace("window.open($1, '', 'popup');", initial_url))) {
      return base::unexpected("window.open failed");
    }
    observer.Wait();

    // Wait for the popup to finish navigating to its initial URL.
    if (!WaitForLoadStop(observer.popup())) {
      return base::unexpected("popup navigation failed");
    }

    if (observer.popup()->GetLastCommittedURL() != final_url) {
      return base::unexpected(absl::StrFormat(
          "popup navigated to %s (expected %s)",
          observer.popup()->GetLastCommittedURL().spec(), final_url.spec()));
    }

    // Wait for the read of the past interaction from the DIPS DB to complete,
    // so the PopupPastInteraction UKM event is reported.
    GetBtmService()->storage()->FlushPostedTasksForTesting();

    return observer.popup();
  }

  // Open a popup window with the given URL, using an ad-tagged script, and
  // return its WebContents.
  base::expected<WebContents*, std::string> OpenAdTaggedPopup(const GURL& url) {
    WebContentsAddedObserver wca_observer;

    ExecuteScriptAsync(GetActiveWebContents(),
                       JsReplace("windowOpenFromAdScript($1)", url));

    WebContents* new_web_contents = wca_observer.GetWebContents();
    TestNavigationObserver navigation_observer(new_web_contents);
    navigation_observer.Wait();
    if (!navigation_observer.last_navigation_succeeded()) {
      return base::unexpected("windowOpenFromAdScript failed");
    }

    return new_web_contents;
  }

  // Navigate a (possibly nested) iframe of `parent_frame` to `url`. Return true
  // iff navigation is successful.
  [[nodiscard]] bool NavigateIframeTo(RenderFrameHost* parent_frame,
                                      const GURL& url) {
    TestNavigationObserver load_observer(GetActiveWebContents());
    std::string script = base::StringPrintf(
        "var iframe = document.getElementById('test_iframe');iframe.src='%s';",
        url.spec().c_str());
    if (!ExecJs(parent_frame, script, EXECUTE_SCRIPT_NO_USER_GESTURE)) {
      return false;
    }
    load_observer.Wait();
    return load_observer.last_navigation_succeeded();
  }

  void SimulateMouseClick(WebContents* web_contents) {
    WaitForHitTestData(web_contents->GetPrimaryMainFrame());
    UserActivationObserver observer(web_contents,
                                    web_contents->GetPrimaryMainFrame());
    ::content::SimulateMouseClick(web_contents, 0,
                                  blink::WebMouseEvent::Button::kLeft);
    observer.Wait();
  }

  void SimulateWebAuthenticationAssertion(WebContents* web_contents) {
    WebAuthnAssertionRequestSucceeded(web_contents->GetPrimaryMainFrame());
  }

  void DestroyWebContents(WebContents* web_contents) {
    WebContentsDestroyedWatcher destruction_watcher(web_contents);
    web_contents->Close();
    destruction_watcher.Wait();
  }

  base::expected<OptionalBool, std::string> GetOpenerHasSameSiteIframe(
      ukm::TestUkmRecorder& ukm_recorder,
      const std::string& entry_name) {
    std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
        ukm_recorder.GetEntries(entry_name, {"OpenerHasSameSiteIframe"});
    if (entries.size() != 1) {
      return base::unexpected(
          base::StringPrintf("Expected 1 %s entry, found %zu",
                             entry_name.c_str(), entries.size()));
    }
    return static_cast<OptionalBool>(
        entries[0].metrics["OpenerHasSameSiteIframe"]);
  }

  std::optional<PopupsStateValue> GetPopupState(const GURL& opener_url,
                                                const GURL& popup_url) {
    std::optional<PopupsStateValue> state;

    GetBtmService()
        ->storage()
        ->AsyncCall(&BtmStorage::ReadPopup)
        .WithArgs(GetSiteForBtm(opener_url), GetSiteForBtm(popup_url))
        .Then(base::BindLambdaForTesting(
            [&state](std::optional<PopupsStateValue> db_state) {
              state = db_state;
            }));
    GetBtmService()->storage()->FlushPostedTasksForTesting();

    return state;
  }

  net::EmbeddedTestServer https_server_;
  base::FieldTrialParams tpcd_heuristics_grants_params_;
  base::SimpleTestClock clock_;
  base::test::ScopedFeatureList feature_list_;

 protected:
  void WaitForCookieIssueAndCheck(std::string_view third_party_site,
                                  std::string_view warning,
                                  std::string_view exclusion) {
    CHECK(warning.empty() || exclusion.empty())
        << "inclusion reason and exclusion reason should not co-exist";
    auto is_cookie_issue = [](const base::Value::Dict& params) {
      const std::string* issue_code =
          params.FindStringByDottedPath("issue.code");
      return issue_code && *issue_code == "CookieIssue";
    };

    // Wait for notification of a Cookie Issue.
    base::Value::Dict params = WaitForMatchingNotification(
        "Audits.issueAdded", base::BindRepeating(is_cookie_issue));

    std::string_view reason_name =
        warning.empty() ? "cookieExclusionReasons" : "cookieWarningReasons";
    std::string_view reason_value = warning.empty() ? exclusion : warning;
    std::string partial_expected =
        JsReplace(R"({
            "cookie": {
               "domain": $1,
               "name": "name",
               "path": "/"
            },
            $2: [ $3 ],
            "operation": "ReadCookie",
         })",
                  third_party_site, reason_name, reason_value);

    // Find relevant fields from cookieIssueDetails
    ASSERT_THAT(params.FindDictByDottedPath("issue.details.cookieIssueDetails"),
                testing::Pointee(base::test::DictionaryHasValues(
                    base::test::ParseJsonDict(partial_expected))));

    ClearNotifications();
  }
};

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       RootWindowDoesntHavePopupState) {
  ASSERT_FALSE(GetTabHelper()->popup_observer_for_testing());
}

// TODO(crbug.com/40276065): Test is flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupsWithOpenerHavePopupState \
  DISABLED_PopupsWithOpenerHavePopupState
#else
#define MAYBE_PopupsWithOpenerHavePopupState PopupsWithOpenerHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupsWithOpenerHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents);
  ASSERT_TRUE(ExecJs(web_contents,
                     JsReplace("window.open($1, '', 'popup');", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
}

// TODO(crbug.com/40925352): Flaky on android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupsWithoutOpenerDoNotHavePopupState \
  DISABLED_PopupsWithoutOpenerDoNotHavePopupState
#else
#define MAYBE_PopupsWithoutOpenerDoNotHavePopupState \
  PopupsWithoutOpenerDoNotHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupsWithoutOpenerDoNotHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents);
  ASSERT_TRUE(
      ExecJs(web_contents,
             JsReplace("window.open($1, '', 'popup,noopener');", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_FALSE(popup_tab_helper->popup_observer_for_testing());
}

// TODO(crbug.com/40925352): Flaky on android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NewTabURLsHavePopupState DISABLED_NewTabURLsHavePopupState
#else
#define MAYBE_NewTabURLsHavePopupState NewTabURLsHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_NewTabURLsHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_TRUE(ExecJs(web_contents, JsReplace("window.open($1);", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
}

class OpenerHeuristicIframeInitiatorBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<
          ::testing::tuple<EnableForIframeTypes, bool>> {
 public:
  OpenerHeuristicIframeInitiatorBrowserTest()
      : iframe_types_flag_(std::get<0>(GetParam())),
        is_nested_iframe_(std::get<1>(GetParam())) {
    for (const auto [val, str] :
         content_settings::features::kEnableForIframeTypesOptions) {
      if (val == iframe_types_flag_) {
        tpcd_heuristics_grants_params_
            ["TpcdPopupHeuristicEnableForIframeInitiator"] = str;
      }
    }
  }

  const EnableForIframeTypes iframe_types_flag_;
  const bool is_nested_iframe_;
};

IN_PROC_BROWSER_TEST_P(
    OpenerHeuristicIframeInitiatorBrowserTest,
    URLsInitiatedByFirstPartyIframes_HavePopupStateWithFlag) {
  WebContents* web_contents = GetActiveWebContents();
  const GURL opener_primary_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  const GURL opener_iframe_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(web_contents, opener_primary_frame_url));
  RenderFrameHost* parent_frame = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(NavigateIframeTo(parent_frame, opener_primary_frame_url));
  if (is_nested_iframe_) {
    parent_frame = ChildFrameAt(parent_frame, 0);
    ASSERT_TRUE(NavigateIframeTo(parent_frame, opener_iframe_url));
  }

  PopupObserver observer(web_contents,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_TRUE(ExecJs(ChildFrameAt(parent_frame, 0),
                     JsReplace("window.open($1);", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);

  if (iframe_types_flag_ == EnableForIframeTypes::kNone) {
    ASSERT_FALSE(popup_tab_helper->popup_observer_for_testing());
  } else {
    ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
  }
}

IN_PROC_BROWSER_TEST_P(
    OpenerHeuristicIframeInitiatorBrowserTest,
    URLsInitiatedByThirdPartyIframes_HavePopupStateWithFlag) {
  WebContents* web_contents = GetActiveWebContents();
  const GURL opener_1p_frame_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  const GURL opener_3p_frame_url =
      embedded_test_server()->GetURL("b.test", "/page_with_blank_iframe.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(web_contents, opener_1p_frame_url));
  RenderFrameHost* parent_frame = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(NavigateIframeTo(parent_frame, opener_3p_frame_url));
  if (is_nested_iframe_) {
    parent_frame = ChildFrameAt(parent_frame, 0);
    ASSERT_TRUE(NavigateIframeTo(parent_frame, opener_1p_frame_url));
  }

  PopupObserver observer(web_contents,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_TRUE(ExecJs(ChildFrameAt(parent_frame, 0),
                     JsReplace("window.open($1);", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);

  if (iframe_types_flag_ == EnableForIframeTypes::kAll) {
    ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
  } else {
    ASSERT_FALSE(popup_tab_helper->popup_observer_for_testing());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OpenerHeuristicIframeInitiatorBrowserTest,
    ::testing::Combine(::testing::Values(EnableForIframeTypes::kNone,
                                         EnableForIframeTypes::kFirstParty,
                                         EnableForIframeTypes::kAll),
                       ::testing::Bool()));

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsReported_WithoutInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  // Note: no previous interaction on a.test.

  ASSERT_THAT(OpenPopup(popup_url), HasValue());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                              {"HoursSinceLastInteraction", "InteractionType"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  // Since there was no prior or current interaction, the
  // HoursSinceLastInteraction field is set to -1.
  EXPECT_EQ(entries[0].metrics["HoursSinceLastInteraction"], -1);
  EXPECT_EQ(entries[0].metrics["InteractionType"],
            static_cast<int32_t>(BtmInteractionType::NoInteraction));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsReported_WithoutRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  RecordUserActivationInteraction(GURL("https://a.test"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_THAT(OpenPopup(popup_url), HasValue());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                              {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // Since the user landed on the page the popup was opened to, the UKM event
  // has source type NAVIGATION_ID.
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

class OpenerHeuristicMultiplePastInteractionTypesBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<InteractionTimesTestCase> {
 public:
  OpenerHeuristicMultiplePastInteractionTypesBrowserTest() = default;

  void MaybeRecordUserActivation(GURL url) {
    int hours_since_activation;
    ASSERT_TRUE(base::StringToInt(GetParam().hours_since_activation_time,
                                  &hours_since_activation));
    if (hours_since_activation >= 0) {
      RecordUserActivationInteraction(
          url, clock_.Now() - base::Hours(hours_since_activation));
    }
  }

  void MaybeRecordUserAuthentication(GURL url) {
    int hours_since_authentication;
    ASSERT_TRUE(base::StringToInt(GetParam().hours_since_authentication_time,
                                  &hours_since_authentication));
    if (hours_since_authentication >= 0) {
      RecordAuthenticationInteraction(
          url, clock_.Now() - base::Hours(hours_since_authentication));
    }
  }

  void getExpectedHoursSinceLastInteraction(
      int& expected_hours_since_last_interaction) const {
    ASSERT_TRUE(base::StringToInt(GetParam().expected_hours_since_interaction,
                                  &expected_hours_since_last_interaction));
  }

  void SetUpOnMainThread() override {
    OpenerHeuristicBrowserTest::SetUpOnMainThread();

    browser_client_.emplace();
    browser_client()->SetBlockThirdPartyCookiesByDefault(true);
  }

  TpcBlockingBrowserClient* browser_client() {
    return &browser_client_->impl();
  }

 private:
  // browser_client_ is wrapped in optional<> to delay construction -- it won't
  // be registered properly if it's created too early.
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

IN_PROC_BROWSER_TEST_P(OpenerHeuristicMultiplePastInteractionTypesBrowserTest,
                       PopupPastInteractionIsReported_MostRecentInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  MaybeRecordUserActivation(GURL("https://a.test"));
  MaybeRecordUserAuthentication(GURL("https://a.test"));

  ASSERT_THAT(OpenPopup(popup_url), HasValue());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                              {"HoursSinceLastInteraction"});
  // Only one interaction is recorded
  ASSERT_EQ(entries.size(), 1u);

  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  // Only most recent interaction was reported regardless of interaction type
  int expected_hours_since_last_interaction = 0;
  getExpectedHoursSinceLastInteraction(expected_hours_since_last_interaction);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction",
                               expected_hours_since_last_interaction)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenerHeuristicMultiplePastInteractionTypesBrowserTest,
                         ::testing::ValuesIn(InteractionTimeTestCases));

// chrome/browser/ui/browser.h (for changing profile prefs) is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)
class OpenerHeuristicPastInteractionGrantBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<AccessGrantTestCase> {
 public:
  OpenerHeuristicPastInteractionGrantBrowserTest() {
    tpcd_heuristics_grants_params_
        ["TpcdWritePopupPastInteractionHeuristicsGrants"] =
            GetParam().write_grant_enabled ? "20m" : "0s";
    tpcd_heuristics_grants_params_
        ["TpcdPopupHeuristicDisableForAdTaggedPopups"] =
            base::ToString(GetParam().disable_for_ad_tagged_popups);
  }

  void SetUpOnMainThread() override {
    OpenerHeuristicBrowserTest::SetUpOnMainThread();

    browser_client_.emplace();
    browser_client()->SetBlockThirdPartyCookiesByDefault(true);
  }

  TpcBlockingBrowserClient* browser_client() {
    return &browser_client_->impl();
  }

 private:
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

namespace {
bool IsFullCookieAccessAllowed(WebContents* web_contents,
                               const GURL& url,
                               const GURL& first_party_url) {
  return GetContentClientForTesting()->browser()->IsFullCookieAccessAllowed(
      web_contents->GetBrowserContext(), web_contents, url,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(first_party_url)),
      /*overrides=*/{});
}
}  // namespace

IN_PROC_BROWSER_TEST_P(OpenerHeuristicPastInteractionGrantBrowserTest,
                       PopupPastInteractionIsReported_WithStorageAccessGrant) {
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL initial_url = embedded_test_server()->GetURL(
      "b.test", "/cross-site/c.test/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  RecordUserActivationInteraction(initial_url, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_THAT(OpenPopup(initial_url, final_url), HasValue());

  // Expect that cookie access was granted for the Popup With Past Interaction
  // heuristic, if the feature is enabled.
  EXPECT_EQ(IsFullCookieAccessAllowed(GetActiveWebContents(), initial_url,
                                      opener_url),
            GetParam().write_grant_enabled);
  // Cookie access was NOT granted for the site that the popup redirected to.
  EXPECT_FALSE(
      IsFullCookieAccessAllowed(GetActiveWebContents(), final_url, opener_url));
}

// TODO: crbug.com/376625002 - disabled for the move to //content since
// SubresourceFilterBrowserTest is unavailable. Either find a way to implement
// this test in //content or move it back to //chrome.
//
// TODO(crbug.com/40947612) Flaky on mac.
IN_PROC_BROWSER_TEST_P(
    OpenerHeuristicPastInteractionGrantBrowserTest,
    DISABLED_AdTaggedPopupPastInteractionIsReported_WithStorageAccessGrant) {
  GURL opener_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  GURL popup_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  RecordUserActivationInteraction(popup_url, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_THAT(OpenAdTaggedPopup(popup_url), HasValue());

  // Expect that cookie access was granted for the ad-tagged Popup With Past
  // Interaction heuristic, only if the flag is *off*.
  bool should_cookies_be_blocked = !GetParam().write_grant_enabled ||
                                   GetParam().disable_for_ad_tagged_popups;
  EXPECT_EQ(
      IsFullCookieAccessAllowed(GetActiveWebContents(), popup_url, opener_url),
      !should_cookies_be_blocked);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenerHeuristicPastInteractionGrantBrowserTest,
                         ::testing::ValuesIn(kAccessGrantTestCases));
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40918571): Test is flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupPastInteractionIsReported_ServerRedirect \
  DISABLED_PopupPastInteractionIsReported_ServerRedirect
#else
#define MAYBE_PopupPastInteractionIsReported_ServerRedirect \
  PopupPastInteractionIsReported_ServerRedirect
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupPastInteractionIsReported_ServerRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url =
      embedded_test_server()->GetURL("a.test", "/server-redirect?title1.html");
  GURL final_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  RecordUserActivationInteraction(GURL("https://a.test"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_THAT(OpenPopup(popup_url, final_url), HasValue());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                              {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // Server redirect causes the UKM event to have source type REDIRECT_ID.
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::REDIRECT_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

// TODO(crbug.com/40282438): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupPastInteractionIsReported_ClientRedirect \
  DISABLED_PopupPastInteractionIsReported_ClientRedirect
#else
#define MAYBE_PopupPastInteractionIsReported_ClientRedirect \
  PopupPastInteractionIsReported_ClientRedirect
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupPastInteractionIsReported_ClientRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  RecordUserActivationInteraction(GURL("https://a.test"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url));
  // Perform a client-side redirect.
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(popup, final_url));

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                              {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // With a client redirect, we still get a source of type NAVIGATION_ID (since
  // the URL committed).
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsReportedOnlyOnce) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  RecordUserActivationInteraction(GURL("https://a.test"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url));

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);

  ASSERT_TRUE(NavigateToURL(
      popup, embedded_test_server()->GetURL("b.test", "/title1.html")));

  // After another navigation, PopupPastInteraction isn't reported again (i.e.,
  // still once total).
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);
}

class OpenerHeuristicInteractionTypesBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<InteractionTypeTestCase> {
 public:
  OpenerHeuristicInteractionTypesBrowserTest() = default;

  void RecordPastInteraction(const GURL& url, base::Time time) {
    if (GetParam() == InteractionTypeTestCase::USER_ACTIVATION) {
      RecordUserActivationInteraction(url, time);
    } else if (GetParam() == InteractionTypeTestCase::WEB_AUTHENTICATION) {
      RecordAuthenticationInteraction(url, time);
    }
  }

  void SimulateInteraction(WebContents* web_contents) {
    if (GetParam() == InteractionTypeTestCase::USER_ACTIVATION) {
      SimulateMouseClick(web_contents);
    } else if (GetParam() == InteractionTypeTestCase::WEB_AUTHENTICATION) {
      SimulateWebAuthenticationAssertion(web_contents);
    }
  }

  bool isAuthenticationInteraction() {
    return GetParam() == InteractionTypeTestCase::WEB_AUTHENTICATION;
  }

  void SetUpOnMainThread() override {
    OpenerHeuristicBrowserTest::SetUpOnMainThread();

    browser_client_.emplace();
    browser_client()->SetBlockThirdPartyCookiesByDefault(true);
  }

  TpcBlockingBrowserClient* browser_client() {
    return &browser_client_->impl();
  }

  base::Time Hours;
  base::Time UserAuthenticationTime;

 private:
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OpenerHeuristicInteractionTypesBrowserTest,
                         ::testing::ValuesIn(InteractionTypeTestCases));

IN_PROC_BROWSER_TEST_P(OpenerHeuristicInteractionTypesBrowserTest,
                       PopupPastInteractionIsFollowedByPostPopupCookieAccess) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  browser_client()->AllowThirdPartyCookiesOnSite(opener_url);

  // Initialize interaction and popup.
  RecordPastInteraction(popup_url, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_THAT(OpenPopup(popup_url), HasValue());
  GetBtmService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM events and DIPS entries were recorded.
  int64_t access_id;
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);
  auto top_level_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"AccessId"});
  ASSERT_EQ(top_level_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(top_level_entries[0].source_id)->url(),
      opener_url);
  access_id = top_level_entries[0].metrics["AccessId"];

  base::OnceCallback<void(std::optional<PopupsStateValue>)> assert_popup =
      base::BindLambdaForTesting([&](std::optional<PopupsStateValue> state) {
        ASSERT_TRUE(state.has_value());
        EXPECT_EQ(access_id, static_cast<int64_t>(state->access_id));
      });
  GetBtmService()
      ->storage()
      ->AsyncCall(&BtmStorage::ReadPopup)
      .WithArgs(GetSiteForBtm(opener_url), GetSiteForBtm(popup_url))
      .Then(std::move(assert_popup));
  GetBtmService()->storage()->FlushPostedTasksForTesting();

  // Add a cookie access by popup_url on opener_url.
  ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(), &https_server_,
                                  "sub.b.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/true));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  CreateImageAndWaitForCookieAccess(
      GetActiveWebContents(),
      https_server_.GetURL("sub.b.test", "/favicon/icon.png?isad=1"));
  GetBtmService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM event for the PostPopupCookieAccess was recorded.
  auto access_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PostPopupCookieAccess",
      {"AccessId", "AccessSucceeded", "IsAdTagged", "HoursSincePopupOpened"});
  ASSERT_EQ(access_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(access_entries[0].source_id)->url(),
      opener_url);
  EXPECT_EQ(access_entries[0].metrics["AccessId"], access_id);
  EXPECT_EQ(access_entries[0].metrics["AccessSucceeded"], true);
  // TODO: crbug.com/376625002 - disabled for the move to //content since
  // SubresourceFilterBrowserTest is unavailable. Either find a way to implement
  // this test in //content or move it back to //chrome.
  // EXPECT_EQ(access_entries[0].metrics["IsAdTagged"],
  //           static_cast<int32_t>(OptionalBool::kTrue));
  EXPECT_EQ(access_entries[0].metrics["HoursSincePopupOpened"], 0);
}

IN_PROC_BROWSER_TEST_P(OpenerHeuristicInteractionTypesBrowserTest,
                       PopupInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL redirect_url =
      embedded_test_server()->GetURL("b.test", "/server-redirect?title1.html");
  GURL final_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url));

  clock_.Advance(base::Minutes(1));
  ASSERT_TRUE(NavigateToURL(popup, redirect_url, final_url));

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      0u);

  clock_.Advance(base::Minutes(1));
  SimulateInteraction(popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries(
          "OpenerHeuristic.PopupInteraction",
          {"SecondsSinceCommitted", "UrlIndex", "InteractionType"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            final_url);
  // The time between *popup_url* committing and the click.
  EXPECT_EQ(entries[0].metrics["SecondsSinceCommitted"],
            Bucketize3PCDHeuristicSample(base::Minutes(2).InSeconds(),
                                         base::Minutes(3).InSeconds()));
  // The user clicked on *final_url*, which was the third URL.
  EXPECT_EQ(entries[0].metrics["UrlIndex"], 3);
  EXPECT_EQ(entries[0].metrics["InteractionType"],
            isAuthenticationInteraction()
                ? static_cast<int32_t>(BtmInteractionType::Authentication)
                : static_cast<int32_t>(BtmInteractionType::UserActivation));
}

// chrome/browser/ui/browser.h (for changing profile prefs) is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)
class OpenerHeuristicCurrentInteractionGrantBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<AccessGrantTestCase> {
 public:
  OpenerHeuristicCurrentInteractionGrantBrowserTest() {
    tpcd_heuristics_grants_params_
        ["TpcdWritePopupCurrentInteractionHeuristicsGrants"] =
            GetParam().write_grant_enabled ? "20m" : "0s";
    tpcd_heuristics_grants_params_
        ["TpcdPopupHeuristicDisableForAdTaggedPopups"] =
            base::ToString(GetParam().disable_for_ad_tagged_popups);
  }

  void SetUpOnMainThread() override {
    OpenerHeuristicBrowserTest::SetUpOnMainThread();

    browser_client_.emplace();
    browser_client()->SetBlockThirdPartyCookiesByDefault(true);
  }

  TpcBlockingBrowserClient* browser_client() {
    return &browser_client_->impl();
  }

 private:
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

IN_PROC_BROWSER_TEST_P(OpenerHeuristicCurrentInteractionGrantBrowserTest,
                       PopupInteractionWithStorageAccessGrant) {
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL initial_url = embedded_test_server()->GetURL(
      "b.test", "/cross-site/c.test/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(initial_url, final_url));
  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);

  // Cookie access was NOT granted for the initial URL (that the user didn't
  // interact with).
  EXPECT_FALSE(IsFullCookieAccessAllowed(GetActiveWebContents(), initial_url,
                                         opener_url));
  // Cookie access WAS granted for the interacted-with URL (if the feature was
  // enabled).
  EXPECT_EQ(
      IsFullCookieAccessAllowed(GetActiveWebContents(), final_url, opener_url),
      GetParam().write_grant_enabled);
}

// TODO: crbug.com/376625002 - disabled for the move to //content since
// SubresourceFilterBrowserTest is unavailable. Either find a way to implement
// this test in //content or move it back to //chrome.
IN_PROC_BROWSER_TEST_P(
    OpenerHeuristicCurrentInteractionGrantBrowserTest,
    DISABLED_AdTaggedPopupInteractionWithStorageAccessGrant) {
  GURL opener_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  GURL popup_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenAdTaggedPopup(popup_url));

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);

  // Expect that cookie access was granted for the ad-tagged Popup With Current
  // Interaction heuristic, only if the flag is *off*.
  bool should_cookies_be_blocked = !GetParam().write_grant_enabled ||
                                   GetParam().disable_for_ad_tagged_popups;
  EXPECT_EQ(
      IsFullCookieAccessAllowed(GetActiveWebContents(), popup_url, opener_url),
      !should_cookies_be_blocked);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenerHeuristicCurrentInteractionGrantBrowserTest,
                         ::testing::ValuesIn(kAccessGrantTestCases));
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupInteractionIsOnlyReportedOnce) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL interaction_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");

  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url));

  ASSERT_TRUE(NavigateToURL(popup, interaction_url));
  SimulateMouseClick(popup);

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);

  SimulateWebAuthenticationAssertion(popup);

  // The second interaction on same URL is not reported.
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);

  ASSERT_TRUE(NavigateToURL(popup, final_url));
  SimulateMouseClick(popup);

  // An additional click on a different site is not reported (still only 1
  // total).
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupInteraction_IgnoreUncommitted) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL uncommitted_url = embedded_test_server()->GetURL("c.test", "/nocontent");

  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url));

  clock_.Advance(base::Minutes(1));
  // Attempt a navigation which won't commit (because the HTTP response is No
  // Content).
  ASSERT_TRUE(NavigateToURL(popup, uncommitted_url, popup_url));

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupInteraction",
                              {"SecondsSinceCommitted", "UrlIndex"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  // The uncommitted navigation was ignored. UrlIndex is still 1.
  EXPECT_EQ(entries[0].metrics["SecondsSinceCommitted"],
            Bucketize3PCDHeuristicSample(base::Minutes(2).InSeconds(),
                                         base::Minutes(3).InSeconds()));
  EXPECT_EQ(entries[0].metrics["UrlIndex"], 1);
}

// Very flaky on macOS: https://crbug.com/40933721
#if BUILDFLAG(IS_MAC)
#define MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess \
  DISABLED_PopupInteraction_IsFollowedByPostPopupCookieAccess
#else
#define MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess \
  PopupInteraction_IsFollowedByPostPopupCookieAccess
#endif
IN_PROC_BROWSER_TEST_P(
    OpenerHeuristicInteractionTypesBrowserTest,
    MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url_1 = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL popup_url_2 =
      embedded_test_server()->GetURL("b.test", "/server-redirect?title1.html");
  GURL popup_url_3 = embedded_test_server()->GetURL("b.test", "/title1.html");
  browser_client()->AllowThirdPartyCookiesOnSite(opener_url);

  // Initialize popup and interaction.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url_1));

  clock_.Advance(base::Minutes(1));
  ASSERT_TRUE(NavigateToURL(popup, popup_url_2, popup_url_3));

  clock_.Advance(base::Minutes(1));
  SimulateInteraction(popup);
  GetBtmService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM events and DIPS entries were recorded.
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);
  auto top_level_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"AccessId"});
  ASSERT_EQ(top_level_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(top_level_entries[0].source_id)->url(),
      opener_url);

  int64_t access_id;
  base::OnceCallback<void(std::optional<PopupsStateValue>)> assert_popup =
      base::BindLambdaForTesting([&](std::optional<PopupsStateValue> state) {
        ASSERT_TRUE(state.has_value());
        access_id = static_cast<int64_t>(state->access_id);
      });
  GetBtmService()
      ->storage()
      ->AsyncCall(&BtmStorage::ReadPopup)
      .WithArgs(GetSiteForBtm(opener_url), GetSiteForBtm(popup_url_3))
      .Then(std::move(assert_popup));
  GetBtmService()->storage()->FlushPostedTasksForTesting();

  // Add a cookie access by popup_url on opener_url.
  ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(), &https_server_,
                                  "sub.b.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  CreateImageAndWaitForCookieAccess(
      GetActiveWebContents(),
      https_server_.GetURL("sub.b.test", "/favicon/icon.png"));
  GetBtmService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM event for the PostPopupCookieAccess was recorded.
  auto access_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PostPopupCookieAccess",
      {"AccessId", "AccessSucceeded", "IsAdTagged", "HoursSincePopupOpened"});
  ASSERT_EQ(access_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(access_entries[0].source_id)->url(),
      opener_url);
  EXPECT_EQ(access_entries[0].metrics["AccessId"], access_id);
  EXPECT_EQ(access_entries[0].metrics["AccessSucceeded"], true);
  EXPECT_EQ(access_entries[0].metrics["IsAdTagged"],
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(access_entries[0].metrics["HoursSincePopupOpened"], 0);
}

// TODO(https://crbug.com/40933721): flaky on Mac.
//
// TODO: crbug.com/376625002 - disabled for the move to //content since the
// DevTools integration is still only in //chrome. Either find a way to
// implement this test in //content or move it back to //chrome.
IN_PROC_BROWSER_TEST_F(
    OpenerHeuristicBrowserTest,
    DISABLED_PopupInteraction_CookieAccessEmitsDevtoolsWarning) {
  GURL opener_url = https_server_.GetURL("a.test", "/title1.html");
  GURL popup_url_1 = https_server_.GetURL("c.test", "/title1.html");
  GURL popup_url_2 =
      https_server_.GetURL("b.test", "/server-redirect?title1.html");
  GURL popup_url_3 = https_server_.GetURL("b.test", "/title1.html");

  // Initialize popup and interaction.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url_1));

  clock_.Advance(base::Minutes(1));
  ASSERT_TRUE(NavigateToURL(popup, popup_url_2, popup_url_3));

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);

  // Add a cookie access by popup_url on opener_url.
  ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(), &https_server_,
                                  "sub.b.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));

  CreateImageAndWaitForCookieAccess(
      GetActiveWebContents(),
      https_server_.GetURL("sub.b.test", "/favicon/icon.png"));

  // CookieIssue was fired since it was exempt from blocking
  WaitForCookieIssueAndCheck("sub.b.test",
                             /*warning=*/{"WarnThirdPartyCookieHeuristic"},
                             /*exclusion=*/{});
}

// TODO: crbug.com/376625002 - disabled for the move to //content since the
// DevTools integration is still only in //chrome. Either find a way to
// implement this test in //content or move it back to //chrome.
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       DISABLED_PopupInteraction_DevtoolsDisableHeuristics) {
  GURL opener_url = https_server_.GetURL("a.test", "/title1.html");
  GURL popup_url_1 = https_server_.GetURL("c.test", "/title1.html");
  GURL popup_url_2 =
      https_server_.GetURL("b.test", "/server-redirect?title1.html");
  GURL popup_url_3 = https_server_.GetURL("b.test", "/title1.html");

  // Initialize popup and interaction.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url_1));

  clock_.Advance(base::Minutes(1));
  ASSERT_TRUE(NavigateToURL(popup, popup_url_2, popup_url_3));

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);

  // Add a cookie access by popup_url on opener_url.
  ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(), &https_server_,
                                  "sub.b.test",
                                  /*is_secure_cookie_set=*/true,
                                  /*is_ad_tagged=*/false));

  SendCommandAsync("Network.enable");
  base::Value::Dict command_params;
  command_params.Set("enableThirdPartyCookieRestriction", true);
  command_params.Set("disableThirdPartyCookieMetadata", false);
  command_params.Set("disableThirdPartyCookieHeuristics", true);
  SendCommandSync("Network.setCookieControls", std::move(command_params));

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  CreateImageAndWaitForCookieAccess(
      GetActiveWebContents(),
      https_server_.GetURL("sub.b.test", "/favicon/icon.png"));

  // Since the cookie is no longer exempted by heuristics,
  // ExcludeThirdPartyPhaseout cookie issue should be present.
  WaitForCookieIssueAndCheck("sub.b.test", /*warning=*/{},
                             /*exclusion=*/{"ExcludeThirdPartyPhaseout"});
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevelIsReported_PastInteraction_NoSameSiteIframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordUserActivationInteraction(GURL("https://b.test"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_TRUE(NavigateToURL(web_contents, toplevel_url));
  ASSERT_THAT(OpenPopup(popup_url), HasValue());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"HasSameSiteIframe", "IsAdTaggedPopupClick"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(entries[0].metrics["IsAdTaggedPopupClick"], false);

  ASSERT_THAT(GetOpenerHasSameSiteIframe(
                  ukm_recorder, "OpenerHeuristic.PopupPastInteraction"),
              ValueIs(OptionalBool::kFalse));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevelIsReported_HasSameSiteIframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  GURL iframe_url =
      embedded_test_server()->GetURL("sub.b.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  const std::string iframe_id = "test_iframe";
  WebContents* web_contents = GetActiveWebContents();

  RecordUserActivationInteraction(GURL("https://b.test"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_TRUE(NavigateToURL(web_contents, toplevel_url));
  ASSERT_TRUE(
      NavigateIframeToURL(GetActiveWebContents(), iframe_id, iframe_url));
  ASSERT_THAT(OpenPopup(popup_url), HasValue());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kTrue));

  ASSERT_THAT(GetOpenerHasSameSiteIframe(
                  ukm_recorder, "OpenerHeuristic.PopupPastInteraction"),
              ValueIs(OptionalBool::kTrue));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest, TopLevel_PopupProvider) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("google.com", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordUserActivationInteraction(GURL("https://google.com"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_TRUE(NavigateToURL(web_contents, toplevel_url));
  ASSERT_THAT(OpenPopup(popup_url), HasValue());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupProvider"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["PopupProvider"],
            static_cast<int64_t>(PopupProvider::kGoogle));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest, TopLevel_PopupId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("google.com", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordUserActivationInteraction(GURL("https://google.com"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_TRUE(NavigateToURL(web_contents, toplevel_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url));

  SimulateMouseClick(popup);

  // Verify all three events share the same popup id.
  auto tl_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupId"});
  ASSERT_EQ(tl_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(tl_entries[0].source_id)->url(),
            toplevel_url);
  const int64_t popup_id = tl_entries[0].metrics["PopupId"];
  EXPECT_NE(popup_id, 0);

  auto pi_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupInteraction", {"PopupId"});
  ASSERT_EQ(pi_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(pi_entries[0].source_id)->url(),
            popup_url);
  EXPECT_EQ(pi_entries[0].metrics["PopupId"], popup_id);

  auto ppi_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PopupPastInteraction", {"PopupId"});
  ASSERT_EQ(ppi_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(ppi_entries[0].source_id)->url(),
            popup_url);
  EXPECT_EQ(ppi_entries[0].metrics["PopupId"], popup_id);

  // Open second popup, verify different popup id.
  ASSERT_THAT(OpenPopup(popup_url), HasValue());
  tl_entries = ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupId"});
  ASSERT_EQ(tl_entries.size(), 2u);
  const int64_t popup_id2 = tl_entries[1].metrics["PopupId"];
  EXPECT_NE(popup_id2, 0);
  EXPECT_NE(popup_id, popup_id2);
}

// TODO: crbug.com/376625002 - disabled for the move to //content since
// SubresourceFilterBrowserTest is unavailable. Either find a way to implement
// this test in //content or move it back to //chrome.
//
// TODO(crbug.com/41484288): Flaky on mac.
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       DISABLED_TopLevel_PastInteraction_AdTagged) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  GURL popup_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  RecordUserActivationInteraction(GURL("https://b.com"),
                                  clock_.Now() - base::Hours(3));

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), toplevel_url));
  ASSERT_THAT(OpenAdTaggedPopup(popup_url), HasValue());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"IsAdTaggedPopupClick"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["IsAdTaggedPopupClick"], true);
}

// TODO: crbug.com/376625002 - disabled for the move to //content since
// SubresourceFilterBrowserTest is unavailable. Either find a way to implement
// this test in //content or move it back to //chrome.
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       DISABLED_TopLevel_CurrentInteraction_AdTagged) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  GURL popup_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), toplevel_url));

  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenAdTaggedPopup(popup_url))
  SimulateMouseClick(popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"IsAdTaggedPopupClick"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["IsAdTaggedPopupClick"], true);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PastAndCurrentInteractionAreBothReportedToDipsDb) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL initial_site = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL initial_url = embedded_test_server()->GetURL(
      "b.test", "/cross-site/c.test/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");

  // Initialize popup and interaction.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), initial_site));
  SimulateMouseClick(GetActiveWebContents());

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(initial_url, final_url));
  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);
  GetBtmService()->storage()->FlushPostedTasksForTesting();

  std::optional<PopupsStateValue> initial_state =
      GetPopupState(opener_url, initial_url);
  std::optional<PopupsStateValue> final_state =
      GetPopupState(opener_url, final_url);
  ASSERT_THAT(
      initial_state,
      Optional(Field(&PopupsStateValue::is_current_interaction, false)));
  ASSERT_THAT(final_state,
              Optional(Field(&PopupsStateValue::is_current_interaction, true)));
}

IN_PROC_BROWSER_TEST_P(OpenerHeuristicInteractionTypesBrowserTest,
                       InteractionTypeIsReportedToDipsDb) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  // GURL initial_site = embedded_test_server()->GetURL("b.test",
  // "/title1.html");
  GURL initial_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  // GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(initial_url));
  clock_.Advance(base::Minutes(1));
  SimulateInteraction(popup);
  GetBtmService()->storage()->FlushPostedTasksForTesting();

  std::optional<PopupsStateValue> initial_state =
      GetPopupState(opener_url, initial_url);

  ASSERT_THAT(initial_state,
              Optional(Field(&PopupsStateValue::is_authentication_interaction,
                             isAuthenticationInteraction())));
}

// chrome/browser/ui/browser.h (for changing profile prefs) is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)
class OpenerHeuristicBackfillGrantBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  OpenerHeuristicBackfillGrantBrowserTest() {
    tpcd_heuristics_grants_params_["TpcdBackfillPopupHeuristicsGrants"] =
        GetParam() ? "1us" : "0s";
    tpcd_heuristics_grants_params_
        ["TpcdWritePopupCurrentInteractionHeuristicsGrants"] = "0s";
  }

  void SetUpOnMainThread() override {
    OpenerHeuristicBrowserTest::SetUpOnMainThread();

    clock_.SetNow(base::Time::Now());

    browser_client_.emplace();
    browser_client()->SetBlockThirdPartyCookiesByDefault(true);
  }

  TpcBlockingBrowserClient* browser_client() {
    return &browser_client_->impl();
  }

 private:
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

// Test the backfill grants created by OpenerHeuristicService when tracking
// protection is onboarded.
IN_PROC_BROWSER_TEST_P(OpenerHeuristicBackfillGrantBrowserTest,
                       TrackingProtectionOnboardingCreatesBackfillGrants) {
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url_1 = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL popup_url_2 = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL popup_url_3 = embedded_test_server()->GetURL("d.test", "/title1.html");

  // popup_url_1 was opened further back than the backfill lookback period of 10
  // minutes.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(popup_url_1));
  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);

  clock_.Advance(base::Minutes(10));

  // popup_url_2 was opened with a past interaction, not a current interaction.
  RecordUserActivationInteraction(popup_url_2, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_THAT(OpenPopup(popup_url_2), HasValue());

  // Only popup_url_3 is eligible for a backfill grant.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(popup, OpenPopup(popup_url_3));
  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);

  base::test::TestFuture<bool> backfill_done;
  GetActiveWebContents()->GetBrowserContext()->BackfillPopupHeuristicGrants(
      backfill_done.GetCallback());
  ASSERT_EQ(backfill_done.Get(), GetParam());

  // Expect that a cookie access grant is not backfilled for popup_url_1 or
  // popup_url_2.
  EXPECT_FALSE(IsFullCookieAccessAllowed(GetActiveWebContents(), popup_url_1,
                                         opener_url));
  EXPECT_FALSE(IsFullCookieAccessAllowed(GetActiveWebContents(), popup_url_2,
                                         opener_url));

  // Expect that a cookie access grant is backfilled for popup_url_3 when the
  // experiment is enabled.
  EXPECT_EQ(IsFullCookieAccessAllowed(GetActiveWebContents(), popup_url_3,
                                      opener_url),
            GetParam());

  // Expect that the cookie access grant applies to other URLs with the same
  // eTLD+1.
  GURL popup_url_3a =
      embedded_test_server()->GetURL("www.d.test", "/favicon.png");
  EXPECT_EQ(IsFullCookieAccessAllowed(GetActiveWebContents(), popup_url_3a,
                                      opener_url),
            GetParam());
  GURL popup_url_3b =
      embedded_test_server()->GetURL("corp.d.test", "/title1.html");
  EXPECT_EQ(IsFullCookieAccessAllowed(GetActiveWebContents(), popup_url_3b,
                                      opener_url),
            GetParam());
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenerHeuristicBackfillGrantBrowserTest,
                         ::testing::Values(false, true));
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
