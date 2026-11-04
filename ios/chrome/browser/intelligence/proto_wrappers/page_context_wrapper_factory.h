// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_FACTORY_H_

#import "base/functional/callback.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"

namespace web {
class WebState;
}  // namespace web

// Abstract factory used to create `PageContextWrapper` instances (or fakes in
// tests).
class PageContextWrapperFactory {
 public:
  using CompletionCallback =
      base::OnceCallback<void(PageContextWrapperCallbackResponse)>;

  virtual ~PageContextWrapperFactory() = default;
  virtual PageContextWrapper* Create(web::WebState* web_state,
                                     PageContextWrapperConfig config,
                                     CompletionCallback completion) = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_FACTORY_H_
