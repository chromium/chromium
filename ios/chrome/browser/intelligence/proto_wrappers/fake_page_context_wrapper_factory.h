// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FAKE_PAGE_CONTEXT_WRAPPER_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FAKE_PAGE_CONTEXT_WRAPPER_FACTORY_H_

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_factory.h"

@class FakePageContextWrapper;

// Factory that creates `FakePageContextWrapper` instances for unit tests.
class FakePageContextWrapperFactory final : public PageContextWrapperFactory {
 public:
  FakePageContextWrapperFactory();
  ~FakePageContextWrapperFactory() override;

  // Returns the most recently created `FakePageContextWrapper` on this factory
  // instance, or `nil` if none has been created.
  FakePageContextWrapper* last_wrapper() { return last_wrapper_; }

  // PageContextWrapperFactory:
  PageContextWrapper* Create(web::WebState* web_state,
                             PageContextWrapperConfig config,
                             CompletionCallback completion) final;

 private:
  FakePageContextWrapper* last_wrapper_ = nil;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FAKE_PAGE_CONTEXT_WRAPPER_FACTORY_H_
