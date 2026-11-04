// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

@implementation FakePageContextWrapper {
  std::optional<PageContextWrapperConfig> _config;
  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      _completionCallback;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                          config:(PageContextWrapperConfig)config
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  self = [super initWithWebState:webState
                          config:config
              completionCallback:base::DoNothing()];
  if (self) {
    _config = std::move(config);
    _completionCallback = std::move(completionCallback);
  }
  return self;
}

- (PageContextWrapperConfig)config {
  return *_config;
}

- (void)populatePageContextFieldsAsync {
  _populateCalled = YES;
}

- (void)populatePageContextFieldsAsyncWithTimeout:(base::TimeDelta)timeout {
  _populateCalled = YES;
  _timeout = timeout;
}

- (void)completeWithResponse:(PageContextWrapperCallbackResponse)response {
  CHECK(_completionCallback);
  std::move(_completionCallback).Run(std::move(response));
}

- (void)completeWithPageContext:
    (std::unique_ptr<optimization_guide::proto::PageContext>)pageContext {
  [self completeWithResponse:base::ok(std::move(pageContext))];
}

- (void)completeWithEmptyPageContext {
  [self completeWithPageContext:std::make_unique<
                                    optimization_guide::proto::PageContext>()];
}

- (void)completeWithError:(PageContextWrapperError)error {
  [self completeWithResponse:base::unexpected(error)];
}

@end
