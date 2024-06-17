// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

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
  return browser->GetSceneState();
}

@end
