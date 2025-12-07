// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_BROWSERTEST_UTILS_H_
#define CONTENT_BROWSER_BTM_BTM_BROWSERTEST_UTILS_H_

#include "base/time/time.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "net/cookies/cookie_setting_override.h"

class GURL;

namespace blink {
class StorageKey;
}

namespace net {
class SchemefulSite;
}

namespace url {
class Origin;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;

// Wraps TpcBlockingBrowserClient for use in content browser tests (which
// require subclasses of ContentBrowserTestContentBrowserClient).
class ContentBrowserTestTpcBlockingBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
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

  bool IsPrivacySandboxReportingDestinationAttested(
      BrowserContext* browser_context,
      const url::Origin& destination_origin,
      PrivacySandboxInvokingAPI invoking_api) override;

  bool IsInterestGroupAPIAllowed(BrowserContext* browser_context,
                                 RenderFrameHost* render_frame_host,
                                 InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override;

  TpcBlockingBrowserClient& impl() { return impl_; }

 private:
  TpcBlockingBrowserClient impl_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_BROWSERTEST_UTILS_H_
