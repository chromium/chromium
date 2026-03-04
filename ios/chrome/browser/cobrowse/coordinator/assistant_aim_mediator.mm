// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

const char kBaseSearchURL[] =
    "https://www.google.com/search?udm=50&sourceid=chrome-mobile&gsc=2&gsas=4";

}  // namespace

@implementation AssistantAIMMediator {
  std::unique_ptr<web::WebState> _webState;
  __weak id<AssistantAIMConsumer> _consumer;
}

@synthesize consumer = _consumer;

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState {
  self = [super init];
  if (self) {
    _webState = std::move(webState);
  }
  return self;
}

- (void)setConsumer:(id<AssistantAIMConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (_consumer && _webState) {
    [_consumer setWebStateView:_webState->GetView()];
    [self loadAIMURL];
  }
}

- (void)disconnect {
  _webState.reset();
}

#pragma mark - Private helpers

// Loads the AIM URL.
// TODO(crbug.com/489118971): Update this function to open the correct AIM
// session.
- (void)loadAIMURL {
  web::NavigationManager::WebLoadParams params(GURL{kBaseSearchURL});
  _webState->GetNavigationManager()->LoadURLWithParams(params);
}

#pragma mark - AssistantAIMMutator

- (void)assistantAIMViewControllerDidRequestSearchWithText:(NSString*)text {
  if (!_webState || text.length == 0) {
    return;
  }

  GURL url = net::AppendQueryParameter(GURL(kBaseSearchURL), "q",
                                       base::SysNSStringToUTF8(text));
  web::NavigationManager::WebLoadParams params{url};
  _webState->GetNavigationManager()->LoadURLWithParams(params);
}

@end
