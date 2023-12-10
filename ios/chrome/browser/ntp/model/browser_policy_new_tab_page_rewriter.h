// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_BROWSER_POLICY_NEW_TAB_PAGE_REWRITER_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_BROWSER_POLICY_NEW_TAB_PAGE_REWRITER_H_

class GURL;

namespace web {
class BrowserState;
}

// If the policy is using a custom New Tab Page URL, replace the default one by
// this  custom one. Returns true if the browser policy new tab page handler
// will handle the URL.
bool WillHandleWebBrowserNewTabPageURLForPolicy(
    GURL* url,
    web::BrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_BROWSER_POLICY_NEW_TAB_PAGE_REWRITER_H_
