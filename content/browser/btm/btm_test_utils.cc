// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_test_utils.h"

#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/btm_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-shared.h"

namespace content {

void CloseTab(WebContents* web_contents) {
  WebContentsDestroyedWatcher destruction_watcher(web_contents);
  web_contents->Close();
  destruction_watcher.Wait();
}

base::expected<WebContents*, std::string> OpenInNewTab(
    WebContents* original_tab,
    const GURL& url) {
  OpenedWindowObserver tab_observer(original_tab,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB);
  if (!ExecJs(original_tab, JsReplace("window.open($1, '_blank');", url))) {
    return base::unexpected("window.open failed");
  }
  tab_observer.Wait();

  // Wait for the new tab to finish navigating.
  WaitForLoadStop(tab_observer.window());

  return tab_observer.window();
}

[[nodiscard]] testing::AssertionResult AccessStorage(
    RenderFrameHost* frame,
    blink::mojom::StorageTypeAccessed type) {
  // We drop the first character of ToString(type) because it's just the
  // constant-indicating 'k'.
  return ExecJs(frame,
                base::StringPrintf(kStorageAccessScript,
                                   base::ToString(type).substr(1).c_str()),
                EXECUTE_SCRIPT_NO_USER_GESTURE,
                /*world_id=*/1);
}

void AccessCookieViaJSIn(WebContents* web_contents, RenderFrameHost* frame) {
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'foo=bar';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
}

[[nodiscard]] testing::AssertionResult ClientSideRedirectViaMetaTag(
    WebContents* web_contents,
    RenderFrameHost* frame,
    const GURL& target_url,
    const std::optional<const GURL>& expected_commit_url) {
  TestFrameNavigationObserver nav_observer(frame);
  bool js_succeeded = ExecJs(frame,
                             JsReplace(
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
                             EXECUTE_SCRIPT_NO_USER_GESTURE);
  if (!js_succeeded) {
    return testing::AssertionFailure()
           << "Failed to execute script to client-side redirect to URL "
           << target_url;
  }
  nav_observer.Wait();
  if (nav_observer.last_committed_url() ==
      expected_commit_url.value_or(target_url)) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "Expected to arrive at "
           << expected_commit_url.value_or(target_url)
           << " but URL was actually " << nav_observer.last_committed_url();
  }
}

[[nodiscard]] testing::AssertionResult ClientSideRedirectViaJS(
    WebContents* web_contents,
    RenderFrameHost* frame,
    const GURL& target_url,
    const std::optional<const GURL>& expected_commit_url) {
  TestFrameNavigationObserver nav_observer(frame);
  bool js_succeeded =
      ExecJs(frame, JsReplace(R"(window.location.replace($1);)", target_url),
             EXECUTE_SCRIPT_NO_USER_GESTURE);
  if (!js_succeeded) {
    return testing::AssertionFailure()
           << "Failed to execute script to client-side redirect to URL "
           << target_url;
  }
  nav_observer.Wait();
  if (nav_observer.last_committed_url() ==
      expected_commit_url.value_or(target_url)) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "Expected to arrive at "
           << expected_commit_url.value_or(target_url)
           << " but URL was actually " << nav_observer.last_committed_url();
  }
}

std::string StringifyBtmClientRedirectMethod(
    BtmClientRedirectMethod client_redirect_method) {
  switch (client_redirect_method) {
    case BtmClientRedirectMethod::kMetaTag:
      return "MetaTag";
    case BtmClientRedirectMethod::kJsWindowLocationReplace:
      return "JsWindowLocationReplace";
    case BtmClientRedirectMethod::kRedirectLikeNavigation:
      return "RedirectLikeNavigation";
  }
}

[[nodiscard]] testing::AssertionResult PerformClientRedirect(
    BtmClientRedirectMethod redirect_method,
    WebContents* web_contents,
    const GURL& redirect_url,
    const std::optional<const GURL>& expected_commit_url) {
  const GURL& commit_url = expected_commit_url.value_or(redirect_url);
  switch (redirect_method) {
    case BtmClientRedirectMethod::kMetaTag:
      return ClientSideRedirectViaMetaTag(web_contents,
                                          web_contents->GetPrimaryMainFrame(),
                                          redirect_url, commit_url);
    case BtmClientRedirectMethod::kJsWindowLocationReplace:
      return ClientSideRedirectViaJS(web_contents,
                                     web_contents->GetPrimaryMainFrame(),
                                     redirect_url, commit_url);
    case BtmClientRedirectMethod::kRedirectLikeNavigation:
      return testing::AssertionResult(
          NavigateToURLFromRendererWithoutUserGesture(
              web_contents, redirect_url, commit_url));
  }
}

bool NavigateToSetCookie(WebContents* web_contents,
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
  bool success = NavigateToURL(web_contents, url);
  if (success) {
    observer.Wait();
  }
  return success;
}

void CreateImageAndWaitForCookieAccess(WebContents* web_contents,
                                       const GURL& image_url) {
  URLCookieAccessObserver observer(web_contents, image_url,
                                   CookieOperation::kRead);
  ASSERT_TRUE(ExecJs(web_contents,
                     JsReplace(
                         R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                         image_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  // The image must cause a cookie access, or else this will hang.
  observer.Wait();
}

std::optional<StateValue> GetBtmState(BtmServiceImpl* btm_service,
                                      const GURL& url) {
  std::optional<StateValue> state;

  auto* storage = btm_service->storage();
  DCHECK(storage);
  storage->AsyncCall(&BtmStorage::Read)
      .WithArgs(url)
      .Then(base::BindLambdaForTesting([&](const BtmState& loaded_state) {
        if (loaded_state.was_loaded()) {
          state = loaded_state.ToStateValue();
        }
      }));
  WaitOnStorage(btm_service);

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
  cookie_accessed_in_primary_page_ = IsInPrimaryPage(*render_frame_host);

  if (details.type == access_type_ && details.url == url_) {
    run_loop_.Quit();
  }
}

void URLCookieAccessObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const CookieAccessDetails& details) {
  cookie_accessed_in_primary_page_ = IsInPrimaryPage(*navigation_handle);

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
    RenderFrameHost* render_frame_host,
    const CookieAccessDetails& details) {
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
  // Sort the URLs before comparing, so order doesn't matter. (BtmDatabase
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

ScopedInitBtmFeature::ScopedInitBtmFeature(bool enable,
                                           const base::FieldTrialParams& params)
    : init_feature_(features::kBtm, enable, params) {}

OpenedWindowObserver::OpenedWindowObserver(
    WebContents* web_contents,
    WindowOpenDisposition open_disposition)
    : WebContentsObserver(web_contents), open_disposition_(open_disposition) {}

void OpenedWindowObserver::DidOpenRequestedURL(
    WebContents* new_contents,
    RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (!window_ && disposition == open_disposition_) {
    window_ = new_contents;
    run_loop_.Quit();
  }
}

// TODO - crbug.com/40247129: Remove this method in favor of directly using
// SimulateMouseClickAndWait() once mouse clicks / taps reliably trigger user
// activation on Android
void SimulateUserActivation(WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(ExecJs(web_contents, ""));
#else
  SimulateMouseClickAndWait(web_contents);
#endif
}

void SimulateMouseClickAndWait(WebContents* web_contents) {
  WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
}

UrlAndSourceId MakeUrlAndId(std::string_view url) {
  return UrlAndSourceId(GURL(url), ukm::AssignNewSourceId());
}

TpcBlockingBrowserClient::TpcBlockingBrowserClient() = default;
TpcBlockingBrowserClient::~TpcBlockingBrowserClient() = default;

bool TpcBlockingBrowserClient::IsFullCookieAccessAllowed(
    BrowserContext* browser_context,
    WebContents* web_contents,
    const GURL& url,
    const blink::StorageKey& storage_key,
    net::CookieSettingOverrides overrides) {
  return IsFullCookieAccessAllowed(url, storage_key.ToNetSiteForCookies(),
                                   storage_key.origin(), overrides,
                                   storage_key.ToCookiePartitionKey());
}

void TpcBlockingBrowserClient::GrantCookieAccessDueToHeuristic(
    BrowserContext* browser_context,
    const net::SchemefulSite& top_frame_site,
    const net::SchemefulSite& accessing_site,
    base::TimeDelta ttl,
    bool ignore_schemes) {
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLToSchemefulSitePattern(
          accessing_site.GetURL());
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::FromURLToSchemefulSitePattern(
          top_frame_site.GetURL());
  if (ignore_schemes) {
    primary_pattern =
        ContentSettingsPattern::ToHostOnlyPattern(primary_pattern);
    secondary_pattern =
        ContentSettingsPattern::ToHostOnlyPattern(secondary_pattern);
  }
  tpc_content_settings_.SetValue(primary_pattern, secondary_pattern,
                                 base::Value(CONTENT_SETTING_ALLOW),
                                 /*metadata=*/{});
}

bool TpcBlockingBrowserClient::AreThirdPartyCookiesGenerallyAllowed(
    BrowserContext* browser_context,
    WebContents* web_contents) {
  return !block_3pcs_;
}

bool TpcBlockingBrowserClient::ShouldBtmDeleteInteractionRecords(
    uint64_t remove_mask) {
  return remove_mask & TpcBlockingBrowserClient::DATA_TYPE_HISTORY;
}

void TpcBlockingBrowserClient::AllowThirdPartyCookiesOnSite(const GURL& url) {
  tpc_content_settings_.SetValue(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(url),
      base::Value(CONTENT_SETTING_ALLOW), /*metadata=*/{});
}

void TpcBlockingBrowserClient::GrantCookieAccessTo3pSite(const GURL& url) {
  tpc_content_settings_.SetValue(
      ContentSettingsPattern::FromURLToSchemefulSitePattern(url),
      ContentSettingsPattern::Wildcard(), base::Value(CONTENT_SETTING_ALLOW),
      /*metadata=*/{});
}

void TpcBlockingBrowserClient::BlockThirdPartyCookiesOnSite(const GURL& url) {
  tpc_content_settings_.SetValue(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(url),
      base::Value(CONTENT_SETTING_BLOCK), /*metadata=*/{});
}

void TpcBlockingBrowserClient::BlockThirdPartyCookies(
    const GURL& url,
    const GURL& first_party_url) {
  tpc_content_settings_.SetValue(
      ContentSettingsPattern::FromURLToSchemefulSitePattern(url),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(first_party_url),
      base::Value(CONTENT_SETTING_BLOCK), /*metadata=*/{});
}

// Overrides for content_settings::CookieSettingsBase

bool TpcBlockingBrowserClient::ShouldIgnoreSameSiteRestrictions(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies) const {
  return false;
}

ContentSetting TpcBlockingBrowserClient::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    content_settings::SettingInfo* info) const {
  if (content_type != ContentSettingsType::COOKIES) {
    return CONTENT_SETTING_DEFAULT;
  }

  const content_settings::RuleEntry* rule_entry =
      tpc_content_settings_.Find(primary_url, secondary_url);
  if (rule_entry == nullptr) {
    if (info) {
      info->primary_pattern = ContentSettingsPattern::Wildcard();
      info->secondary_pattern = ContentSettingsPattern::Wildcard();
    }
    // By default we'll allow cookies. Blocking by default will override any 3PC
    // exemption settings.
    return CONTENT_SETTING_ALLOW;
  }
  if (info) {
    info->primary_pattern = rule_entry->first.primary_pattern;
    info->secondary_pattern = rule_entry->first.secondary_pattern;
  }
  return content_settings::ValueToContentSetting(rule_entry->second.value);
}

bool TpcBlockingBrowserClient::ShouldAlwaysAllowCookies(
    const GURL& url,
    const GURL& first_party_url) const {
  return false;
}

bool TpcBlockingBrowserClient::ShouldBlockThirdPartyCookies(
    base::optional_ref<const url::Origin> top_frame_origin,
    net::CookieSettingOverrides overrides) const {
  return block_3pcs_;
}

bool TpcBlockingBrowserClient::MitigationsEnabledFor3pcd() const {
  return false;
}

bool TpcBlockingBrowserClient::IsThirdPartyCookiesAllowedScheme(
    const std::string& scheme) const {
  return false;
}

PausedCookieAccessObservers::PausedCookieAccessObservers(
    NotifyCookiesAccessedCallback callback,
    PendingObserversWithContext observers)
    : CookieAccessObservers(std::move(callback)),
      pending_receivers_(std::move(observers)) {}

PausedCookieAccessObservers::~PausedCookieAccessObservers() = default;

void PausedCookieAccessObservers::Add(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> receiver,
    CookieAccessDetails::Source source) {
  pending_receivers_.emplace_back(std::move(receiver), source);
}

PausedCookieAccessObservers::PendingObserversWithContext
PausedCookieAccessObservers::TakeReceiversWithContext() {
  return std::exchange(pending_receivers_, {});
}

CookieAccessInterceptor::CookieAccessInterceptor(WebContents& web_contents)
    : WebContentsObserver(&web_contents) {}

CookieAccessInterceptor::~CookieAccessInterceptor() = default;

void CookieAccessInterceptor::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  auto& request = *NavigationRequest::From(navigation_handle);

  auto observers = std::make_unique<PausedCookieAccessObservers>(
      base::BindRepeating(&NavigationRequest::NotifyCookiesAccessed,
                          // Unretained is safe here because ownership of the
                          // observers is passed to the request below.
                          base::Unretained(&request)),
      request.TakeCookieObservers());
  request.SetCookieAccessObserversForTesting(std::move(observers));
}

}  // namespace content
