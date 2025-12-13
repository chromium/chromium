// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_UTILS_H_

namespace web {
class WebState;
}  // namespace web

// Returns true if the context for `web_state` can be extracted. PageContexts
// are available for HTML and image pages that use http/https schemes. Namely,
// this filters out PDFs, NTPs and chrome:// pages.
bool CanExtractPageContextForWebState(web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_UTILS_H_
