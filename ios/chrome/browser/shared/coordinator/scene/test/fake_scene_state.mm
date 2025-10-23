// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"

#import "base/apple/foundation_util.h"
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
  std::unique_ptr<TestBrowser> _inactive_browser;
  std::unique_ptr<TestBrowser> _incognito_browser;
  // Used to check that -shutdown is called before -dealloc.
  BOOL _shutdown;
}

@synthesize browserProviderInterface = _browserProviderInterface;

@synthesize window = _window;
@synthesize appState = _appState;

- (instancetype)initWithAppState:(AppState*)appState
                         profile:(ProfileIOS*)profile {
  if ((self = [super initWithAppState:appState])) {
    DCHECK(profile);
    DCHECK(!profile->IsOffTheRecord());
    self.activationLevel = SceneActivationLevelForegroundInactive;
    self.browserProviderInterface = [[StubBrowserProviderInterface alloc] init];
    self.appState = appState;

    _browser = std::make_unique<TestBrowser>(profile, self);
    base::apple::ObjCCastStrict<StubBrowserProvider>(
        self.browserProviderInterface.mainBrowserProvider)
        .browser = _browser.get();

    _inactive_browser = std::make_unique<TestBrowser>(profile, self);
    base::apple::ObjCCastStrict<StubBrowserProvider>(
        self.browserProviderInterface.mainBrowserProvider)
        .inactiveBrowser = _inactive_browser.get();

    _incognito_browser =
        std::make_unique<TestBrowser>(profile->GetOffTheRecordProfile(), self);
    base::apple::ObjCCastStrict<StubBrowserProvider>(
        self.browserProviderInterface.incognitoBrowserProvider)
        .browser = _incognito_browser.get();
  }
  return self;
}

- (void)dealloc {
  CHECK(_shutdown) << "-shutdown must be called before -dealloc";
}

- (void)appendWebStateWithURL:(const GURL)URL {
  auto test_web_state = std::make_unique<web::FakeWebState>();
  test_web_state->SetCurrentURL(URL);
  WebStateList* web_state_list =
      self.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList();
  web_state_list->InsertWebState(std::move(test_web_state));
}

- (void)appendWebStatesWithURL:(const GURL)URL count:(int)count {
  for (int i = 0; i < count; i++) {
    [self appendWebStateWithURL:URL];
  }
}

- (void)shutdown {
  _incognito_browser.reset();
  _inactive_browser.reset();
  _browser.reset();
  _shutdown = YES;
}

@end
