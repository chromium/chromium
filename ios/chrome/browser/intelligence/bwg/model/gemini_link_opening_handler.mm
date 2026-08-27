// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_link_opening_handler.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"

@implementation GeminiLinkOpeningHandler {
  // The URL loading agent for opening URLs.
  raw_ptr<UrlLoadingBrowserAgent> _URLLoadingAgent;

  // The command dispatcher to dispatch commands.
  CommandDispatcher* _dispatcher;
}

#pragma mark - BWGLinkOpeningDelegate

- (instancetype)initWithURLLoader:(UrlLoadingBrowserAgent*)URLLoadingAgent
                       dispatcher:(CommandDispatcher*)dispatcher {
  self = [super self];
  if (self) {
    _URLLoadingAgent = URLLoadingAgent;
    _dispatcher = dispatcher;
  }
  return self;
}

- (void)disconnect {
  _URLLoadingAgent = nullptr;
  _dispatcher = nil;
}

- (void)openURLInNewTab:(NSString*)URL {
  [self openURL:URL closePresentedViews:NO];
}

- (void)closePresentedViewsAndOpenURLInNewTab:(NSString*)URL {
  [self openURL:URL closePresentedViews:YES];
}

- (void)openURL:(NSString*)URL closePresentedViews:(BOOL)closePresentedViews {
  GURL gurl = GURL(base::SysNSStringToUTF8(URL));
  if (!gurl.is_valid()) {
    return;
  }

  if (closePresentedViews) {
    id<SceneCommands> sceneCommandsHandler =
        HandlerForProtocol(_dispatcher, SceneCommands);
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:gurl];
    [sceneCommandsHandler closePresentedViewsAndOpenURL:command];
  } else {
    if (!_URLLoadingAgent) {
      return;
    }
    UrlLoadParams params = UrlLoadParams::InNewTab(gurl);
    params.append_to = OpenPosition::kCurrentTab;
    _URLLoadingAgent->Load(params);
  }

  RecordURLOpened();

  id<GeminiCommands> geminiHandler =
      HandlerForProtocol(_dispatcher, GeminiCommands);
  [geminiHandler minimizeGeminiIfInvoked];
}

@end
