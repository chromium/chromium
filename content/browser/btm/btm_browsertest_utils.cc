// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_browsertest_utils.h"

#include "net/cookies/cookie_setting_override.h"

namespace content {

bool ContentBrowserTestTpcBlockingBrowserClient::IsFullCookieAccessAllowed(
    BrowserContext* browser_context,
    WebContents* web_contents,
    const GURL& url,
    const blink::StorageKey& storage_key,
    net::CookieSettingOverrides overrides) {
  return impl_.IsFullCookieAccessAllowed(browser_context, web_contents, url,
                                         storage_key, overrides);
}

bool ContentBrowserTestTpcBlockingBrowserClient::
    AreThirdPartyCookiesGenerallyAllowed(BrowserContext* browser_context,
                                         WebContents* web_contents) {
  return impl_.AreThirdPartyCookiesGenerallyAllowed(browser_context,
                                                    web_contents);
}

void ContentBrowserTestTpcBlockingBrowserClient::
    GrantCookieAccessDueToHeuristic(BrowserContext* browser_context,
                                    const net::SchemefulSite& top_frame_site,
                                    const net::SchemefulSite& accessing_site,
                                    base::TimeDelta ttl,
                                    bool ignore_schemes) {
  impl_.GrantCookieAccessDueToHeuristic(browser_context, top_frame_site,
                                        accessing_site, ttl, ignore_schemes);
}

bool ContentBrowserTestTpcBlockingBrowserClient::
    ShouldBtmDeleteInteractionRecords(uint64_t remove_mask) {
  return impl_.ShouldBtmDeleteInteractionRecords(remove_mask);
}

bool ContentBrowserTestTpcBlockingBrowserClient::
    IsPrivacySandboxReportingDestinationAttested(
        BrowserContext* browser_context,
        const url::Origin& destination_origin,
        PrivacySandboxInvokingAPI invoking_api) {
  return true;
}

bool ContentBrowserTestTpcBlockingBrowserClient::IsInterestGroupAPIAllowed(
    BrowserContext* browser_context,
    RenderFrameHost* render_frame_host,
    InterestGroupApiOperation operation,
    const url::Origin& top_frame_origin,
    const url::Origin& api_origin) {
  return true;
}

}  // namespace content
