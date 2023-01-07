// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_URL_REWRITERS_URL_REWRITERS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_URL_REWRITERS_URL_REWRITERS_API_H_

namespace web {
class BrowserURLRewriter;
}  // namespace web

namespace ios {
namespace provider {

// Registers the URL rewriters to `rewriter`.
void AddURLRewriters(web::BrowserURLRewriter* rewriter);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_URL_REWRITERS_URL_REWRITERS_API_H_
