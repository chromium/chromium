// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/dips_test_utils.h"

#include <string_view>

#include "base/test/bind.h"
#include "content/browser/dips/dips_service_impl.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/dips_delegate.h"
#include "content/public/browser/dips_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::CookieAccessDetails;
using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;

void CloseTab(content::WebContents* web_contents) {
  content::WebContentsDestroyedWatcher destruction_watcher(web_contents);
  web_contents->Close();
  destruction_watcher.Wait();
}

base::expected<WebContents*, std::string> OpenInNewTab(
    WebContents* original_tab,
    const GURL& url) {
  OpenedWindowObserver tab_observer(original_tab,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB);
  if (!content::ExecJs(original_tab,
                       content::JsReplace("window.open($1, '_blank');", url))) {
    return base::unexpected("window.open failed");
  }
  tab_observer.Wait();

  // Wait for the new tab to finish navigating.
  content::WaitForLoadStop(tab_observer.window());

  return tab_observer.window();
}

void AccessCookieViaJSIn(content::WebContents* web_contents,
                         content::RenderFrameHost* frame) {
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
}

[[nodiscard]] testing::AssertionResult ClientSideRedirectViaMetaTag(
    content::WebContents* web_contents,
    content::RenderFrameHost* frame,
    const GURL& target_url) {
  content::TestFrameNavigationObserver nav_observer(frame);
  bool js_succeeded = content::ExecJs(frame,
                                      content::JsReplace(
                                          R"(
      function redirectViaMetaTag() {
        var element = document.createElement('meta');
        element.setAttribute('http-equiv', 'refresh');
        element.setAttribute('content', '0; url=$1');
        document.getElementsByTagName('head')[0].appendChild(element);
      }
      if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", redirectViaMetaTag);
      } else {
        redirectViaMetaTag();
      }
      )",
                                          target_url),
                                      content::EXECUTE_SCRIPT_NO_USER_GESTURE);
  if (!js_succeeded) {
    return testing::AssertionFailure()
           << "Failed to execute script to client-side redirect to URL "
           << target_url;
  }
  nav_observer.Wait();
  if (nav_observer.last_committed_url() == target_url) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "Expected to arrive at " << target_url << " but URL was actually "
           << nav_observer.last_committed_url();
  }
}

[[nodiscard]] testing::AssertionResult ClientSideRedirectViaJS(
    content::WebContents* web_contents,
    content::RenderFrameHost* frame,
    const GURL& target_url) {
  content::TestFrameNavigationObserver nav_observer(frame);
  bool js_succeeded = content::ExecJs(
      frame, content::JsReplace(R"(window.location.replace($1);)", target_url),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE);
  if (!js_succeeded) {
    return testing::AssertionFailure()
           << "Failed to execute script to client-side redirect to URL "
           << target_url;
  }
  nav_observer.Wait();
  if (nav_observer.last_committed_url() == target_url) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "Expected to arrive at " << target_url << " but URL was actually "
           << nav_observer.last_committed_url();
  }
}

bool NavigateToSetCookie(content::WebContents* web_contents,
                         const net::EmbeddedTestServer* server,
                         std::string_view host,
                         bool is_secure_cookie_set,
                         bool is_ad_tagged) {
  std::string relative_url = "/set-cookie?name=value";
  if (is_secure_cookie_set) {
    relative_url += ";Secure;SameSite=None";
  }
  if (is_ad_tagged) {
    relative_url += "&isad=1";
  }
  const auto url = server->GetURL(host, relative_url);

  URLCookieAccessObserver observer(web_contents, url, CookieOperation::kChange);
  bool success = content::NavigateToURL(web_contents, url);
  if (success) {
    observer.Wait();
  }
  return success;
}

void CreateImageAndWaitForCookieAccess(content::WebContents* web_contents,
                                       const GURL& image_url) {
  URLCookieAccessObserver observer(web_contents, image_url,
                                   CookieOperation::kRead);
  ASSERT_TRUE(content::ExecJs(web_contents,
                              content::JsReplace(
                                  R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                                  image_url),
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  // The image must cause a cookie access, or else this will hang.
  observer.Wait();
}

std::optional<StateValue> GetDIPSState(DIPSServiceImpl* dips_service,
                                       const GURL& url) {
  std::optional<StateValue> state;

  auto* storage = dips_service->storage();
  DCHECK(storage);
  storage->AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(base::BindLambdaForTesting([&](const DIPSState& loaded_state) {
        if (loaded_state.was_loaded()) {
          state = loaded_state.ToStateValue();
        }
      }));
  WaitOnStorage(dips_service);

  return state;
}

URLCookieAccessObserver::URLCookieAccessObserver(WebContents* web_contents,
                                                 const GURL& url,
                                                 CookieOperation access_type)
    : WebContentsObserver(web_contents), url_(url), access_type_(access_type) {}

void URLCookieAccessObserver::Wait() {
  run_loop_.Run();
}

void URLCookieAccessObserver::OnCookiesAccessed(
    RenderFrameHost* render_frame_host,
    const CookieAccessDetails& details) {
  cookie_accessed_in_primary_page_ = IsInPrimaryPage(render_frame_host);

  if (details.type == access_type_ && details.url == url_) {
    run_loop_.Quit();
  }
}

void URLCookieAccessObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const CookieAccessDetails& details) {
  cookie_accessed_in_primary_page_ = IsInPrimaryPage(navigation_handle);

  if (details.type == access_type_ && details.url == url_) {
    run_loop_.Quit();
  }
}

bool URLCookieAccessObserver::CookieAccessedInPrimaryPage() const {
  return cookie_accessed_in_primary_page_;
}

FrameCookieAccessObserver::FrameCookieAccessObserver(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    CookieOperation access_type)
    : WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host),
      access_type_(access_type) {}

void FrameCookieAccessObserver::Wait() {
  run_loop_.Run();
}

void FrameCookieAccessObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (details.type == access_type_ && render_frame_host_ == render_frame_host) {
    run_loop_.Quit();
  }
}

UserActivationObserver::UserActivationObserver(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host)
    : WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host) {}

void UserActivationObserver::Wait() {
  run_loop_.Run();
}

void UserActivationObserver::FrameReceivedUserActivation(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host_ == render_frame_host) {
    run_loop_.Quit();
  }
}

EntryUrlsAre::EntryUrlsAre(std::string entry_name,
                           std::vector<std::string> urls)
    : entry_name_(std::move(entry_name)), expected_urls_(std::move(urls)) {
  // Sort the URLs before comparing, so order doesn't matter. (DIPSDatabase
  // currently sorts its results, but that could change and these tests
  // shouldn't care.)
  std::sort(expected_urls_.begin(), expected_urls_.end());
}

EntryUrlsAre::EntryUrlsAre(const EntryUrlsAre&) = default;
EntryUrlsAre::EntryUrlsAre(EntryUrlsAre&&) = default;
EntryUrlsAre::~EntryUrlsAre() = default;

bool EntryUrlsAre::MatchAndExplain(
    const ukm::TestUkmRecorder& ukm_recorder,
    testing::MatchResultListener* result_listener) const {
  std::vector<std::string> actual_urls;
  for (const ukm::mojom::UkmEntry* entry :
       ukm_recorder.GetEntriesByName(entry_name_)) {
    GURL url = ukm_recorder.GetSourceForSourceId(entry->source_id)->url();
    actual_urls.push_back(url.spec());
  }
  std::sort(actual_urls.begin(), actual_urls.end());

  // ExplainMatchResult() won't print out the full contents of `actual_urls`,
  // so for more helpful error messages, we do it ourselves.
  *result_listener << "whose entries for '" << entry_name_
                   << "' contain the URLs "
                   << testing::PrintToString(actual_urls) << ", ";

  // Use ContainerEq() instead of e.g. ElementsAreArray() because the error
  // messages are much more useful.
  return ExplainMatchResult(testing::ContainerEq(expected_urls_), actual_urls,
                            result_listener);
}

void EntryUrlsAre::DescribeTo(std::ostream* os) const {
  *os << "has entries for '" << entry_name_ << "' whose URLs are "
      << testing::PrintToString(expected_urls_);
}

void EntryUrlsAre::DescribeNegationTo(std::ostream* os) const {
  *os << "does not have entries for '" << entry_name_ << "' whose URLs are "
      << testing::PrintToString(expected_urls_);
}

ScopedInitFeature::ScopedInitFeature(const base::Feature& feature,
                                     bool enable,
                                     const base::FieldTrialParams& params) {
  if (enable) {
    feature_list_.InitAndEnableFeatureWithParameters(feature, params);
  } else {
    feature_list_.InitAndDisableFeature(feature);
  }
}

ScopedInitDIPSFeature::ScopedInitDIPSFeature(
    bool enable,
    const base::FieldTrialParams& params)
    : init_feature_(features::kDIPS, enable, params) {}

OpenedWindowObserver::OpenedWindowObserver(
    content::WebContents* web_contents,
    WindowOpenDisposition open_disposition)
    : WebContentsObserver(web_contents), open_disposition_(open_disposition) {}

void OpenedWindowObserver::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (!window_ && disposition == open_disposition_) {
    window_ = new_contents;
    run_loop_.Quit();
  }
}

void SimulateMouseClickAndWait(WebContents* web_contents) {
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  content::SimulateMouseClick(web_contents, 0,
                              blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
}

UrlAndSourceId MakeUrlAndId(std::string_view url) {
  return UrlAndSourceId(GURL(url), ukm::AssignNewSourceId());
}

TpcBlockingBrowserClient::TpcBlockingBrowserClient() = default;
TpcBlockingBrowserClient::~TpcBlockingBrowserClient() = default;

bool TpcBlockingBrowserClient::IsFullCookieAccessAllowed(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& url,
    const blink::StorageKey& storage_key) {
  // TODO: crbug.com/384531044 - implement this method by subclassing
  // `content_settings::CookieSettingsBase` and calling its
  // `IsFullCookieAccessAllowed()`.
  const net::SchemefulSite top_level_site = storage_key.top_level_site();
  const net::SchemefulSite url_site(url);

  if (base::Contains(tpc_1p_blocks_, top_level_site)) {
    return false;
  }

  if (base::Contains(tpc_blocks_, std::make_pair(top_level_site, url_site))) {
    return false;
  }

  if (!block_3pcs_) {
    return true;
  }

  if (storage_key.ToNetSiteForCookies().IsFirstParty(url)) {
    return true;
  }

  // XXX how should we use storage_key.origin() ?
  if (base::Contains(tpc_1p_site_exceptions_, top_level_site)) {
    return true;
  }

  if (base::Contains(tpc_3p_site_exceptions_, url_site)) {
    return true;
  }

  if (base::Contains(
          schemeless_tpc_exceptions_,
          std::make_pair(
              top_level_site.registrable_domain_or_host_for_testing(),
              url_site.registrable_domain_or_host_for_testing()))) {
    return true;
  }

  bool b =
      base::Contains(tpc_exceptions_, std::make_pair(top_level_site, url_site));
  return b;
}

void TpcBlockingBrowserClient::GrantCookieAccessDueToHeuristic(
    content::BrowserContext* browser_context,
    const net::SchemefulSite& top_frame_site,
    const net::SchemefulSite& accessing_site,
    base::TimeDelta ttl,
    bool ignore_schemes) {
  if (ignore_schemes) {
    schemeless_tpc_exceptions_.emplace(
        top_frame_site.registrable_domain_or_host_for_testing(),
        accessing_site.registrable_domain_or_host_for_testing());
  } else {
    tpc_exceptions_.emplace(top_frame_site, accessing_site);
  }
}

// A DipsDelegate that only differs from the default (i.e., no delegate)
// behavior in one way: ShouldDeleteInteractionRecords() checks for the
// DATA_TYPE_HISTORY bit.
class SimpleDipsDelegate : public content::DipsDelegate {
 public:
  bool ShouldEnableDips(content::BrowserContext* browser_context) override {
    return true;
  }

  void OnDipsServiceCreated(content::BrowserContext* browser_context,
                            DIPSService* dips_service) override {}

  uint64_t GetRemoveMask() override { return DIPSService::kDefaultRemoveMask; }

  bool ShouldDeleteInteractionRecords(uint64_t remove_mask) override {
    return remove_mask & TpcBlockingBrowserClient::DATA_TYPE_HISTORY;
  }
};

std::unique_ptr<content::DipsDelegate>
TpcBlockingBrowserClient::CreateDipsDelegate() {
  return std::make_unique<SimpleDipsDelegate>();
}

void TpcBlockingBrowserClient::AllowThirdPartyCookiesOnSite(const GURL& url) {
  tpc_1p_site_exceptions_.emplace(url);
}

void TpcBlockingBrowserClient::GrantCookieAccessTo3pSite(const GURL& url) {
  tpc_3p_site_exceptions_.emplace(url);
}

void TpcBlockingBrowserClient::BlockThirdPartyCookiesOnSite(const GURL& url) {
  tpc_1p_blocks_.emplace(url);
}

void TpcBlockingBrowserClient::BlockThirdPartyCookies(
    const GURL& url,
    const GURL& first_party_url) {
  tpc_blocks_.emplace(net::SchemefulSite(first_party_url),
                      net::SchemefulSite(url));
}
