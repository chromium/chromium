// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_new_tab_button.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridNewTabButton () {
  UIImage* _regularImage;
  UIImage* _incognitoImage;
}
@end

@implementation TabGridNewTabButton

- (instancetype)initWithRegularImage:(UIImage*)regularImage
                      incognitoImage:(UIImage*)incognitoImage {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _regularImage = regularImage;
    _incognitoImage = incognitoImage;
  }
  return self;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  // self.page is inited to 0 (i.e. TabGridPageIncognito) so do not early return
  // here, otherwise when app is launched in incognito mode the image will be
  // missing.
  UIImage* renderedImage;
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      renderedImage = [_incognitoImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
      break;
    case TabGridPageRegularTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      renderedImage = [_regularImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
      break;
    case TabGridPageRemoteTabs:
      break;
  }
  _page = page;
  [self setImage:renderedImage forState:UIControlStateNormal];
}

@end
