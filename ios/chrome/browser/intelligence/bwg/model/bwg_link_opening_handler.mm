// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_handler.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_delegate.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "url/gurl.h"

@implementation BWGLinkOpeningHandler {
  // The URL loading agent for opening URLs.
  raw_ptr<UrlLoadingBrowserAgent, DanglingUntriaged> _URLLoadingAgent;

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

- (void)openURLInNewTab:(NSString*)URL {
  [self openURL:URL closePresentedViews:!IsGeminiCopresenceEnabled()];
}

- (void)closePresentedViewsAndOpenURLInNewTab:(NSString*)URL {
  [self openURL:URL closePresentedViews:true];
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
    UrlLoadParams params = UrlLoadParams::InNewTab(gurl);
    params.append_to = OpenPosition::kCurrentTab;
    _URLLoadingAgent->Load(params);
  }

  RecordURLOpened();

  if (!IsGeminiCopresenceEnabled()) {
    return;
  }

  [self.geminiViewStateDelegate
      switchToViewState:ios::provider::GeminiViewState::kCollapsed];
}

@end
