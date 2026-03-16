// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

// Sheet detent update animation duration.
const CGFloat kSheetDetentAnimationDuration = 0.3;

}  // namespace

@implementation AssistantAIMMediator {
  std::unique_ptr<web::WebState> _webState;
  __weak id<AssistantAIMConsumer> _consumer;
  CobrowseContext* _context;
  id<AssistantContainerCommands> _containerHandler;
}

@synthesize consumer = _consumer;

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                         context:(CobrowseContext*)context
                containerHandler:
                    (id<AssistantContainerCommands>)containerHandler {
  self = [super init];
  if (self) {
    _webState = std::move(webState);
    _context = context;
    _containerHandler = containerHandler;
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

// Loads the URL defined in the cobrowse context.
- (void)loadAIMURL {
  [_containerHandler
      animateAssistantContainerToDetent:AssistantContainerDetent::kMedium
                               duration:kSheetDetentAnimationDuration
                                  curve:UIViewAnimationCurveEaseInOut];
  web::NavigationManager::WebLoadParams params(_context.url);
  _webState->GetNavigationManager()->LoadURLWithParams(params);
}

#pragma mark - ComposeboxURLLoader

// Prepares the load for a given query text by appending it to the base URL.
- (void)prepareLoadForQueryText:(NSString*)queryText {
  if (!_webState || queryText.length == 0) {
    return;
  }

  GURL url = net::AppendOrReplaceQueryParameter(
      _context.url, "q", base::SysNSStringToUTF8(queryText));
  web::NavigationManager::WebLoadParams params{url};
  _webState->GetNavigationManager()->LoadURLWithParams(params);

  [self.delegate assistantAIMMediatorDidLoadQuery:self];
}

- (void)loadURLParams:(const UrlLoadParams&)URLLoadParams {
  // NO-OP
}

@end
