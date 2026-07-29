// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"

#import <utility>

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
  // Owning pointer for the BrowserProviderInterface instance.
  StubBrowserProviderInterface* _browserProviderInterface;
  // Owning pointer for the browser that backs the interface provider.
  std::unique_ptr<TestBrowser> _browser;
  std::unique_ptr<TestBrowser> _incognito_browser;
  // Used to check that -shutdown is called before -dealloc.
  BOOL _shutdown;
}

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  if ((self = [super init])) {
    DCHECK(profile);
    DCHECK(!profile->IsOffTheRecord());
    self.activationLevel = SceneActivationLevelForegroundInactive;

    _browser = std::make_unique<TestBrowser>(profile, self);
    std::ignore = _browser->CreateInactiveBrowser();
    _incognito_browser =
        std::make_unique<TestBrowser>(profile->GetOffTheRecordProfile(), self);

    _browserProviderInterface = [[StubBrowserProviderInterface alloc]
         initWithBrowser:_browser.get()
        incognitoBrowser:_incognito_browser.get()];
  }
  return self;
}

- (id<BrowserProviderInterface>)browserProviderInterface {
  return _browserProviderInterface;
}

- (void)dealloc {
  CHECK(_shutdown) << "-shutdown must be called before -dealloc";
}

- (void)setCurrentBrowserProvider:(id<BrowserProvider>)browserProvider {
  CHECK(browserProvider == nil ||
        browserProvider == _browserProviderInterface.mainBrowserProvider ||
        browserProvider == _browserProviderInterface.incognitoBrowserProvider);

  _browserProviderInterface.currentBrowserProvider =
      base::apple::ObjCCastStrict<StubBrowserProvider>(browserProvider);
}

- (void)destroyAndRecreateOffTheRecordProfile {
  // Remember whether the current interface was incognito in order to
  // restore it after the destruction/creation.
  const BOOL currentInterfaceWasIncognito =
      _browserProviderInterface.currentBrowserProvider ==
      _browserProviderInterface.incognitoBrowserProvider;

  [_browserProviderInterface.incognitoBrowserProvider shutdown];
  _browserProviderInterface.incognitoBrowserProvider = nil;

  ProfileIOS* profile = _browser->GetProfile();

  // Destroy the incognito Browser and Profile.
  _incognito_browser.reset();
  profile->DestroyOffTheRecordProfile();
  CHECK(!profile->HasOffTheRecordProfile());

  // Recreate the incognito Browser and Profile (implicitly created when
  // accessed from the Profile after its destruction).
  _incognito_browser =
      std::make_unique<TestBrowser>(profile->GetOffTheRecordProfile(), self);

  StubBrowserProvider* incognitoBrowserProvider =
      [[StubBrowserProvider alloc] initWithBrowser:_incognito_browser.get()];
  _browserProviderInterface.incognitoBrowserProvider = incognitoBrowserProvider;

  if (currentInterfaceWasIncognito) {
    _browserProviderInterface.currentBrowserProvider = incognitoBrowserProvider;
  }
}

- (void)appendWebStateWithURL:(const GURL&)URL {
  auto test_web_state = std::make_unique<web::FakeWebState>();
  test_web_state->SetCurrentURL(URL);

  _browser->GetWebStateList()->InsertWebState(std::move(test_web_state));
}

- (void)appendWebStatesWithURL:(const GURL&)URL count:(int)count {
  for (int i = 0; i < count; i++) {
    [self appendWebStateWithURL:URL];
  }
}

- (void)shutdown {
  [_browserProviderInterface shutdown];
  _browserProviderInterface = nil;

  _incognito_browser.reset();
  _browser.reset();
  _shutdown = YES;
}

@end
