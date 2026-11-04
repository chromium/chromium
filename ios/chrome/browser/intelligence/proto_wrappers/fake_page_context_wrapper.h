// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FAKE_PAGE_CONTEXT_WRAPPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FAKE_PAGE_CONTEXT_WRAPPER_H_

#import <memory>
#import <optional>

#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

// Fake `PageContextWrapper` for unit tests that records calls to populate page
// context without executing JavaScript or starting timers on the `WebState`.
// Extraction remains pending until `-completeWithResponse:` (or one of the
// convenience completion methods) is called.
@interface FakePageContextWrapper : PageContextWrapper

// Whether `-populatePageContextFieldsAsync` or
// `-populatePageContextFieldsAsyncWithTimeout:` was called.
@property(nonatomic, readonly, getter=wasPopulateCalled) BOOL populateCalled;

// The timeout passed to `-populatePageContextFieldsAsyncWithTimeout:`, or
// `std::nullopt` if `-populatePageContextFieldsAsync` was called without an
// explicit timeout.
@property(nonatomic, readonly) std::optional<base::TimeDelta> timeout;

// The configuration passed to `-initWithWebState:config:completionCallback:`.
@property(nonatomic, readonly) PageContextWrapperConfig config;

// Invokes the stored completion callback with `response`.
- (void)completeWithResponse:(PageContextWrapperCallbackResponse)response;

// Invokes the stored completion callback with `pageContext`.
- (void)completeWithPageContext:
    (std::unique_ptr<optimization_guide::proto::PageContext>)pageContext;

// Invokes the stored completion callback with a newly allocated empty
// `PageContext` proto.
- (void)completeWithEmptyPageContext;

// Invokes the stored completion callback with `error`.
- (void)completeWithError:(PageContextWrapperError)error;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_FAKE_PAGE_CONTEXT_WRAPPER_H_
