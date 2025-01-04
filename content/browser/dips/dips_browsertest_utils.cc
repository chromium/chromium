// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/dips_browsertest_utils.h"

#include "content/public/browser/dips_delegate.h"

namespace content {

bool ContentBrowserTestTpcBlockingBrowserClient::IsFullCookieAccessAllowed(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    const GURL& url,
    const blink::StorageKey& storage_key) {
  return impl_.IsFullCookieAccessAllowed(browser_context, web_contents, url,
                                         storage_key);
}

void ContentBrowserTestTpcBlockingBrowserClient::
    GrantCookieAccessDueToHeuristic(content::BrowserContext* browser_context,
                                    const net::SchemefulSite& top_frame_site,
                                    const net::SchemefulSite& accessing_site,
                                    base::TimeDelta ttl,
                                    bool ignore_schemes) {
  impl_.GrantCookieAccessDueToHeuristic(browser_context, top_frame_site,
                                        accessing_site, ttl, ignore_schemes);
}

std::unique_ptr<content::DipsDelegate>
ContentBrowserTestTpcBlockingBrowserClient::CreateDipsDelegate() {
  return impl_.CreateDipsDelegate();
}

bool ContentBrowserTestTpcBlockingBrowserClient::
    IsPrivacySandboxReportingDestinationAttested(
        content::BrowserContext* browser_context,
        const url::Origin& destination_origin,
        content::PrivacySandboxInvokingAPI invoking_api) {
  return true;
}

bool ContentBrowserTestTpcBlockingBrowserClient::IsInterestGroupAPIAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* render_frame_host,
    InterestGroupApiOperation operation,
    const url::Origin& top_frame_origin,
    const url::Origin& api_origin) {
  return true;
}

}  // namespace content
