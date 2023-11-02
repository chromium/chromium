// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/test/fake_scene_state.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeSceneState ()
// Redeclare interface provider readwrite.
@property(nonatomic, strong, readwrite) id<BrowserInterfaceProvider>
    interfaceProvider;

@end

@implementation FakeSceneState {
  // Owning pointer for the browser that backs the interface provider.
  std::unique_ptr<TestBrowser> _browser;
  std::unique_ptr<TestBrowser> _incognito_browser;
}

@synthesize interfaceProvider = _interfaceProvider;

@synthesize window = _window;

- (instancetype)initWithAppState:(AppState*)appState
                    browserState:(ChromeBrowserState*)browserState {
  if (self = [super initWithAppState:appState]) {
    DCHECK(browserState);
    DCHECK(!browserState->IsOffTheRecord());
    self.activationLevel = SceneActivationLevelForegroundInactive;
    self.interfaceProvider = [[StubBrowserInterfaceProvider alloc] init];

    _browser = std::make_unique<TestBrowser>(browserState);
    base::mac::ObjCCastStrict<StubBrowserInterface>(
        self.interfaceProvider.mainInterface)
        .browser = _browser.get();

    _incognito_browser = std::make_unique<TestBrowser>(
        browserState->GetOffTheRecordChromeBrowserState());
    base::mac::ObjCCastStrict<StubBrowserInterface>(
        self.interfaceProvider.incognitoInterface)
        .browser = _incognito_browser.get();
  }
  return self;
}

+ (NSArray<FakeSceneState*>*)sceneArrayWithCount:(int)count
                                    browserState:
                                        (ChromeBrowserState*)browserState {
  NSMutableArray<SceneState*>* scenes = [NSMutableArray array];
  for (int i = 0; i < count; i++) {
    [scenes addObject:[[self alloc] initWithAppState:nil
                                        browserState:browserState]];
  }
  return [scenes copy];
}

- (void)appendWebStateWithURL:(const GURL)URL {
  auto test_web_state = std::make_unique<web::FakeWebState>();
  test_web_state->SetCurrentURL(URL);
  WebStateList* web_state_list =
      self.interfaceProvider.mainInterface.browser->GetWebStateList();
  web_state_list->InsertWebState(
      WebStateList::kInvalidIndex, std::move(test_web_state),
      WebStateList::INSERT_NO_FLAGS, WebStateOpener());
}

- (void)appendWebStatesWithURL:(const GURL)URL count:(int)count {
  for (int i = 0; i < count; i++) {
    [self appendWebStateWithURL:URL];
  }
}

@end
