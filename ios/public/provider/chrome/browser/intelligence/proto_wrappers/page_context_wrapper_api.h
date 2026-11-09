// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_API_H_

#import <string>

class GURL;

namespace ios::provider {

// Returns true if `url` is protected and its page context must not be
// extracted.
bool IsProtectedUrl(const GURL& url);

// Returns the JavaScript snippet body that evaluates whether PageContext
// should be detached from the request for the current frame. Callers wrap this
// snippet inside a JavaScript function body, so it must `return` a boolean.
std::u16string GetPageContextShouldDetachScript();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_API_H_
