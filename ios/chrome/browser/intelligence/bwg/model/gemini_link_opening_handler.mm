// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_link_opening_handler.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@implementation GeminiLinkOpeningHandler {
  // The URL loading agent for opening URLs.
  raw_ptr<UrlLoadingBrowserAgent> _URLLoadingAgent;
}

- (instancetype)initWithURLLoader:(UrlLoadingBrowserAgent*)URLLoadingAgent {
  self = [super init];
  if (self) {
    _URLLoadingAgent = URLLoadingAgent;
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  _URLLoadingAgent = nullptr;
  _geminiHandler = nil;
  _sceneHandler = nil;
}

#pragma mark - BWGLinkOpeningDelegate

- (void)openURLInNewTab:(NSString*)URL {
  [self openURL:URL closePresentedViews:NO];
}

- (void)closePresentedViewsAndOpenURLInNewTab:(NSString*)URL {
  [self openURL:URL closePresentedViews:YES];
}

- (void)openImmersiveLearningCardURLInWebModal:(NSURL*)URL {
  if (ShouldOpenGeminiQuizzesInNewTab()) {
    [self openURLInNewTab:URL.absoluteString];
    return;
  }

  GURL gurl = net::GURLWithNSURL(URL);
  if (!gurl.is_valid() || !_geminiHandler) {
    // `_geminiHandler` is nil after `-disconnect` is called.
    return;
  }

  RecordURLOpened();
  RecordGeminiImmersiveLearningCardWebModalOpened();
  [_geminiHandler showGeminiWebModalForURL:gurl];
}

#pragma mark - Private

// Opens `URL` in a new tab, optionally closing presented views first, and
// minimizes Gemini if invoked.
- (void)openURL:(NSString*)URL closePresentedViews:(BOOL)closePresentedViews {
  GURL gurl = GURL(base::SysNSStringToUTF8(URL));
  if (!gurl.is_valid()) {
    return;
  }

  if (closePresentedViews) {
    if (!_sceneHandler) {
      // `_sceneHandler` is nil after `-disconnect` is called.
      return;
    }
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:gurl];
    [_sceneHandler closePresentedViewsAndOpenURL:command];
  } else {
    if (!_URLLoadingAgent) {
      // `_URLLoadingAgent` is null after `-disconnect` is called.
      return;
    }
    UrlLoadParams params = UrlLoadParams::InNewTab(gurl);
    params.append_to = OpenPosition::kCurrentTab;
    _URLLoadingAgent->Load(params);
  }

  RecordURLOpened();

  [_geminiHandler minimizeGeminiIfInvoked];
}

@end
