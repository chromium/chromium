// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper_factory.h"

#import <utility>

#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper.h"

FakePageContextWrapperFactory::FakePageContextWrapperFactory() = default;

FakePageContextWrapperFactory::~FakePageContextWrapperFactory() = default;

PageContextWrapper* FakePageContextWrapperFactory::Create(
    web::WebState* web_state,
    PageContextWrapperConfig config,
    CompletionCallback completion) {
  last_wrapper_ =
      [[FakePageContextWrapper alloc] initWithWebState:web_state
                                                config:std::move(config)
                                    completionCallback:std::move(completion)];
  return last_wrapper_;
}
