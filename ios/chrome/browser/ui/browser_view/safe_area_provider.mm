// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/safe_area_provider.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SafeAreaProvider ()

// SceneState from the associated Browser object.
@property(nonatomic, readonly) SceneState* sceneState;

@end

@implementation SafeAreaProvider {
  // The current Browser object.
  base::WeakPtr<Browser> _browser;
}

#pragma mark - Public

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _browser = browser->AsWeakPtr();
  }
  return self;
}

- (UIEdgeInsets)safeArea {
  return self.sceneState.window.safeAreaInsets;
}

#pragma mark - Private properties

- (SceneState*)sceneState {
  Browser* browser = _browser.get();
  if (!browser) {
    return nil;
  }
  return SceneStateBrowserAgent::FromBrowser(browser)->GetSceneState();
}

@end
