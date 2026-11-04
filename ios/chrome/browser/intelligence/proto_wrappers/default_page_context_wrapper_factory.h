// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_DEFAULT_PAGE_CONTEXT_WRAPPER_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_DEFAULT_PAGE_CONTEXT_WRAPPER_FACTORY_H_

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_factory.h"

// Default `PageContextWrapperFactory` implementation that creates real
// `PageContextWrapper` instances.
class DefaultPageContextWrapperFactory final
    : public PageContextWrapperFactory {
 public:
  static DefaultPageContextWrapperFactory& GetInstance();

  // PageContextWrapperFactory:
  PageContextWrapper* Create(web::WebState* web_state,
                             PageContextWrapperConfig config,
                             CompletionCallback completion) final;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_DEFAULT_PAGE_CONTEXT_WRAPPER_FACTORY_H_
