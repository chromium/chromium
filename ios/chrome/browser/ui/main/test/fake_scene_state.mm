// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/test/fake_scene_state.h"

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
  UIWindow* _window;
}

@synthesize interfaceProvider = _interfaceProvider;

- (instancetype)initWithAppState:(AppState*)appState {
  if (self = [super initWithAppState:appState]) {
    self.activationLevel = SceneActivationLevelForegroundInactive;
    self.interfaceProvider = [[StubBrowserInterfaceProvider alloc] init];
    StubBrowserInterface* mainInterface = static_cast<StubBrowserInterface*>(
        self.interfaceProvider.mainInterface);
    _browser = std::make_unique<TestBrowser>();
    mainInterface.browser = _browser.get();
  }
  return self;
}

+ (NSArray<FakeSceneState*>*)sceneArrayWithCount:(int)count {
  NSMutableArray<SceneState*>* scenes = [NSMutableArray array];
  for (int i = 0; i < count; i++) {
    [scenes addObject:[[self alloc] initWithAppState:nil]];
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

- (UIWindow*)window {
  return _window;
}

- (void)setWindow:(UIWindow*)window {
  _window = window;
}

@end
