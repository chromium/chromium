// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"

#import <utility>

#import "base/check.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"

@implementation FakeSceneState {
  // Owning pointer for the browser that backs the interface provider.
  std::unique_ptr<TestBrowser> _browser;
  std::unique_ptr<TestBrowser> _incognito_browser;
  // Overridden value for the scene session identifier.
  std::string _sceneSessionID;
  // Used to check that -shutdown is called before -dealloc.
  BOOL _shutdown;
}

@synthesize browserProviderInterface = _browserProviderInterface;

@synthesize window = _window;
@synthesize appState = _appState;

- (instancetype)initWithAppState:(AppState*)appState
                         profile:(ProfileIOS*)profile
                  sceneSessionID:(std::string)sceneSessionID
               commandDispatcher:(CommandDispatcher*)commandDispatcher {
  if ((self = [super initWithAppState:appState])) {
    DCHECK(profile);
    DCHECK(!profile->IsOffTheRecord());
    self.activationLevel = SceneActivationLevelForegroundInactive;
    StubBrowserProviderInterface* browserProviderInterface =
        [[StubBrowserProviderInterface alloc] init];
    self.browserProviderInterface = browserProviderInterface;
    self.appState = appState;

    _browser = std::make_unique<TestBrowser>(profile, self);
    if (commandDispatcher) {
      // Only override the command dispatcher if non-nil (since TestBrowser
      // creates a default command dispatcher in its constructor).
      _browser->SetCommandDispatcher(commandDispatcher);
    }

    browserProviderInterface.mainBrowserProvider.browser = _browser.get();
    std::ignore = _browser->CreateInactiveBrowser();

    _incognito_browser =
        std::make_unique<TestBrowser>(profile->GetOffTheRecordProfile(), self);
    browserProviderInterface.incognitoBrowserProvider.browser =
        _incognito_browser.get();

    _sceneSessionID = std::move(sceneSessionID);
  }
  return self;
}

- (instancetype)initWithAppState:(AppState*)appState
                         profile:(ProfileIOS*)profile
                  sceneSessionID:(std::string)sceneSessionID {
  return [self initWithAppState:appState
                        profile:profile
                 sceneSessionID:std::move(sceneSessionID)
              commandDispatcher:nil];
}

- (instancetype)initWithAppState:(AppState*)appState
                         profile:(ProfileIOS*)profile {
  return [self initWithAppState:appState
                        profile:profile
                 sceneSessionID:{}
              commandDispatcher:nil];
}

- (std::string_view)sceneSessionID {
  return _sceneSessionID.empty() ? [super sceneSessionID] : _sceneSessionID;
}

- (void)dealloc {
  CHECK(_shutdown) << "-shutdown must be called before -dealloc";
}

- (void)appendWebStateWithURL:(const GURL&)URL {
  auto test_web_state = std::make_unique<web::FakeWebState>();
  test_web_state->SetCurrentURL(URL);
  WebStateList* web_state_list =
      self.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  web_state_list->InsertWebState(std::move(test_web_state));
}

- (void)appendWebStatesWithURL:(const GURL&)URL count:(int)count {
  for (int i = 0; i < count; i++) {
    [self appendWebStateWithURL:URL];
  }
}

- (void)shutdown {
  _incognito_browser.reset();
  _browser.reset();
  _shutdown = YES;
}

@end
