// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_TEST_UTILS_H_
#define CONTENT_BROWSER_BTM_BTM_TEST_UTILS_H_

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_utils.h"
#include "content/browser/renderer_host/cookie_access_observers.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/btm_service.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "url/gurl.h"

namespace testing {
class MatchResultListener;
}

namespace content {

constexpr char kStorageAccessScript[] = R"(
    function accessLocalStorage() {
      localStorage.setItem('foo', 'bar');
      return localStorage.getItem('foo');
    }

    function accessSessionStorage() {
      sessionStorage.setItem('foo', 'bar');
      return sessionStorage.getItem('foo') == 'bar';
    }

    async function accessFileSystem() {
      const fs = await new Promise((resolve, reject) => {
        window.webkitRequestFileSystem(TEMPORARY, 1024, resolve, reject);
      });
      return new Promise((resolve, reject) => {
        fs.root.getFile('foo.txt', {create: true, exclusive: true}, resolve,
          reject);
      });
    }

    function accessIndexedDB() {
      var request = indexedDB.open('my_db', 2);

      request.onupgradeneeded = () => {
        request.result.createObjectStore('store');
      }
      return new Promise((resolve) => {
        request.onsuccess = () => {
          request.result.close();
          resolve(true);
        }
        request.onerror = () => {throw new Error('Failed to access!')}
      });
    }

    function accessCacheStorage() {
      return caches.open("cache")
      .then((cache) => cache.put("/foo", new Response("bar")))
      .then(() => true)
      .catch(() => {throw new Error('Failed to access!')});
    }

    // Placeholder for execution statement.
    access%s();
  )";

using StateForURLCallback = base::OnceCallback<void(BtmState)>;

// Helper function to close (and waits for closure of) a `web_contents` tab.
void CloseTab(WebContents* web_contents);

// Helper function to open a link to the given URL in a new tab and return the
// new tab's WebContents.
base::expected<WebContents*, std::string> OpenInNewTab(
    WebContents* original_tab,
    const GURL& url);

[[nodiscard]] testing::AssertionResult AccessStorage(
    RenderFrameHost* frame,
    blink::mojom::StorageTypeAccessed type);

// Helper function for performing client side cookie access via JS.
void AccessCookieViaJSIn(WebContents* web_contents, RenderFrameHost* frame);

// Redirect `frame` in `web_contents` to `target_url` via an HTML `<meta>` tag.
// If `expected_commit_url` is non-null, asserts a final commit URL of
// `expected_commit_url`; otherwise, asserts a final commit URL of `target_url`.
[[nodiscard]] testing::AssertionResult ClientSideRedirectViaMetaTag(
    WebContents* web_contents,
    RenderFrameHost* frame,
    const GURL& target_url,
    const std::optional<const GURL>& expected_commit_url = std::nullopt);

// Redirect `frame` in `web_contents` to `target_url` via a JavaScript call to
// `window.location.replace()`. If `expected_commit_url` is non-null, asserts a
// final commit URL of `expected_commit_url`; otherwise, asserts a final commit
// URL of `target_url`.
[[nodiscard]] testing::AssertionResult ClientSideRedirectViaJS(
    WebContents* web_contents,
    RenderFrameHost* frame,
    const GURL& target_url,
    const std::optional<const GURL>& expected_commit_url = std::nullopt);

enum class BtmClientRedirectMethod : int {
  kMetaTag = 0,
  kJsWindowLocationReplace = 1,
  kRedirectLikeNavigation = 2,
};

const auto kAllBtmClientRedirectMethods =
    testing::Values(BtmClientRedirectMethod::kMetaTag,
                    BtmClientRedirectMethod::kJsWindowLocationReplace,
                    BtmClientRedirectMethod::kRedirectLikeNavigation);

std::string StringifyBtmClientRedirectMethod(BtmClientRedirectMethod method);

// Redirect `web_contents` to `redirect_url` using the client redirect method
// `redirect_method`. Expects the final commit URL to be `expected_commit_url`
// if non-null, or else `redirect_url`.
[[nodiscard]] testing::AssertionResult PerformClientRedirect(
    BtmClientRedirectMethod redirect_method,
    WebContents* web_contents,
    const GURL& redirect_url,
    const std::optional<const GURL>& expected_commit_url = std::nullopt);

// Helper function to navigate to /set-cookie on `host` and wait for
// OnCookiesAccessed() to be called.
bool NavigateToSetCookie(WebContents* web_contents,
                         const net::EmbeddedTestServer* server,
                         std::string_view host,
                         bool is_secure_cookie_set,
                         bool is_ad_tagged);

// Helper function for creating an image with a cookie access on the provided
// WebContents.
void CreateImageAndWaitForCookieAccess(WebContents* web_contents,
                                       const GURL& image_url);

// Helper function to block until all BTM storage requests are complete.
inline void WaitOnStorage(BtmServiceImpl* btm_service) {
  btm_service->storage()->FlushPostedTasksForTesting();
}

// Helper function to query the `url` state from BTM storage.
std::optional<StateValue> GetBtmState(BtmServiceImpl* btm_service,
                                      const GURL& url);

inline BtmServiceImpl* GetBtmService(WebContents* web_contents) {
  return BtmServiceImpl::Get(web_contents->GetBrowserContext());
}

class URLCookieAccessObserver : public WebContentsObserver {
 public:
  URLCookieAccessObserver(WebContents* web_contents,
                          const GURL& url,
                          CookieOperation access_type);

  void Wait();

  bool CookieAccessedInPrimaryPage() const;

 private:
  // WebContentsObserver overrides
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override;

  GURL url_;
  CookieOperation access_type_;
  bool cookie_accessed_in_primary_page_ = false;
  base::RunLoop run_loop_;
};

class FrameCookieAccessObserver : public WebContentsObserver {
 public:
  explicit FrameCookieAccessObserver(WebContents* web_contents,
                                     RenderFrameHost* render_frame_host,
                                     CookieOperation access_type);

  // Wait until the frame accesses cookies.
  void Wait();

  // WebContentsObserver override
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;

 private:
  const raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged>
      render_frame_host_;
  CookieOperation access_type_;
  base::RunLoop run_loop_;
};

class UserActivationObserver : public WebContentsObserver {
 public:
  explicit UserActivationObserver(WebContents* web_contents,
                                  RenderFrameHost* render_frame_host);

  // Wait until the frame receives user activation.
  void Wait();

 private:
  // WebContentsObserver override
  void FrameReceivedUserActivation(RenderFrameHost* render_frame_host) override;

  raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> const
      render_frame_host_;
  base::RunLoop run_loop_;
};

// Checks that the URLs associated with the UKM entries with the given name are
// as expected. Sorts the URLs so order doesn't matter.
//
// Example usage:
//
// EXPECT_THAT(ukm_recorder, EntryUrlsAre(entry_name, {url1, url2, url3}));
class EntryUrlsAre {
 public:
  using is_gtest_matcher = void;
  EntryUrlsAre(std::string entry_name, std::vector<std::string> urls);
  EntryUrlsAre(const EntryUrlsAre&);
  EntryUrlsAre(EntryUrlsAre&&);
  ~EntryUrlsAre();

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  bool MatchAndExplain(const ukm::TestUkmRecorder& ukm_recorder,
                       testing::MatchResultListener* result_listener) const;

  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  std::string entry_name_;
  std::vector<std::string> expected_urls_;
};

// Enables or disables a base::Feature.
class ScopedInitFeature {
 public:
  explicit ScopedInitFeature(const base::Feature& feature,
                             bool enable,
                             const base::FieldTrialParams& params);

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Enables/disables the BTM Feature.
class ScopedInitBtmFeature {
 public:
  explicit ScopedInitBtmFeature(bool enable,
                                const base::FieldTrialParams& params = {});

 private:
  ScopedInitFeature init_feature_;
};

// Waits for a window to open.
class OpenedWindowObserver : public WebContentsObserver {
 public:
  explicit OpenedWindowObserver(WebContents* web_contents,
                                WindowOpenDisposition open_disposition);

  void Wait() { run_loop_.Run(); }
  WebContents* window() { return window_; }

 private:
  // WebContentsObserver overrides:
  void DidOpenRequestedURL(WebContents* new_contents,
                           RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  const WindowOpenDisposition open_disposition_;
  raw_ptr<WebContents> window_ = nullptr;
  base::RunLoop run_loop_;
};

void SimulateUserActivation(WebContents* web_contents);

// Simulate a mouse click and wait for the main frame to receive user
// activation.
void SimulateMouseClickAndWait(WebContents*);

// A ContentBrowserClient that supports third-party cookie blocking. Note that
// this can only be used directly by unit tests; browser tests must use
// ContentBrowserTestTpcBlockingBrowserClient instead.
class TpcBlockingBrowserClient : public ContentBrowserClient,
                                 public content_settings::CookieSettingsBase {
 public:
  using content_settings::CookieSettingsBase::IsFullCookieAccessAllowed;

  static constexpr uint64_t DATA_TYPE_HISTORY =
      BrowsingDataRemover::DATA_TYPE_CONTENT_END << 1;

  TpcBlockingBrowserClient();
  ~TpcBlockingBrowserClient() override;

  void SetBlockThirdPartyCookiesByDefault(bool block) { block_3pcs_ = block; }

  bool IsFullCookieAccessAllowed(
      BrowserContext* browser_context,
      WebContents* web_contents,
      const GURL& url,
      const blink::StorageKey& storage_key,
      net::CookieSettingOverrides overrides) override;

  void GrantCookieAccessDueToHeuristic(BrowserContext* browser_context,
                                       const net::SchemefulSite& top_frame_site,
                                       const net::SchemefulSite& accessing_site,
                                       base::TimeDelta ttl,
                                       bool ignore_schemes) override;

  bool AreThirdPartyCookiesGenerallyAllowed(BrowserContext* browser_context,
                                            WebContents* web_contents) override;

  bool ShouldBtmDeleteInteractionRecords(uint64_t remove_mask) override;

  void AllowThirdPartyCookiesOnSite(const GURL& url);
  void GrantCookieAccessTo3pSite(const GURL& url);

  void BlockThirdPartyCookiesOnSite(const GURL& url);
  void BlockThirdPartyCookies(const GURL& url, const GURL& first_party_url);

  // Overrides for content_settings::CookieSettingsBase

  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override;

  ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      content_settings::SettingInfo* info) const override;

  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const override;

  bool ShouldBlockThirdPartyCookies(
      base::optional_ref<const url::Origin> top_frame_origin,
      net::CookieSettingOverrides overrides) const override;

  bool MitigationsEnabledFor3pcd() const override;

  bool IsThirdPartyCookiesAllowedScheme(std::string_view scheme) const override;

 private:
  bool block_3pcs_ = false;
  content_settings::HostIndexedContentSettings tpc_content_settings_;
};

// Class used to pause cookie access notifications. The class works by unbinding
// existing CookieAccessObserver receivers and storing new ones without binding
// them.
class PausedCookieAccessObservers : public CookieAccessObservers {
 public:
  explicit PausedCookieAccessObservers(NotifyCookiesAccessedCallback callback,
                                       PendingObserversWithContext observers);
  ~PausedCookieAccessObservers() override;

  // CookieAccessObservers
  void Add(mojo::PendingReceiver<network::mojom::CookieAccessObserver> receiver,
           CookieAccessDetails::Source source) override;
  PendingObserversWithContext TakeReceiversWithContext() override;

 private:
  // Holds existing and new receivers.
  PendingObserversWithContext pending_receivers_;
};

// Class used to pause all cookie access notifications in a WebContents.
class CookieAccessInterceptor : public WebContentsObserver {
 public:
  explicit CookieAccessInterceptor(WebContents& web_contents);
  ~CookieAccessInterceptor() override;

  // WebContentsObserver
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_TEST_UTILS_H_
