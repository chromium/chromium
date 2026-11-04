// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/default_page_context_wrapper_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

// static
DefaultPageContextWrapperFactory&
DefaultPageContextWrapperFactory::GetInstance() {
  static base::NoDestructor<DefaultPageContextWrapperFactory> kInstance;
  return *kInstance;
}

PageContextWrapper* DefaultPageContextWrapperFactory::Create(
    web::WebState* web_state,
    PageContextWrapperConfig config,
    CompletionCallback completion) {
  return [[PageContextWrapper alloc] initWithWebState:web_state
                                               config:std::move(config)
                                   completionCallback:std::move(completion)];
}
