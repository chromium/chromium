// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_view.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@implementation TabStripView

@synthesize layoutDelegate = _layoutDelegate;

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.showsHorizontalScrollIndicator = NO;
    self.showsVerticalScrollIndicator = NO;
    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits =
          TraitCollectionSetForTraits(@[ UITraitHorizontalSizeClass.class ]);
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf notifyDelegateOfTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

- (BOOL)touchesShouldCancelInContentView:(UIView*)view {
  // The default implementation returns YES for all views except for UIControls.
  // Override to return YES for UIControls as well.
  return YES;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self.layoutDelegate layoutTabStripSubviews];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self notifyDelegateOfTraitChange:previousTraitCollection];
}
#endif

// Notifies `layoutDelegate` that a change in the collection of UITraits has
// been observed.
- (void)notifyDelegateOfTraitChange:
    (UITraitCollection*)previousTraitCollection {
  [self.layoutDelegate traitCollectionDidChange:previousTraitCollection];
  [self.layoutDelegate layoutTabStripSubviews];
}

@end
