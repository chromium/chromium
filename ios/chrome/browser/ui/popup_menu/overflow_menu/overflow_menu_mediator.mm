// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"

#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

OverflowMenuAction* CreateOverflowMenuAction(int nameID, NSString* imageName) {
  NSString* name = l10n_util::GetNSString(nameID);
  return [[OverflowMenuAction alloc] initWithName:name
                                        imageName:imageName
                               enterpriseDisabled:NO];
}

}  // namespace

@interface OverflowMenuMediator ()

@property(nonatomic, strong) OverflowMenuAction* openIncognitoTabAction;

@end

@implementation OverflowMenuMediator

@synthesize overflowMenuModel = _overflowMenuModel;

- (OverflowMenuModel*)overflowMenuModel {
  if (!_overflowMenuModel) {
    _overflowMenuModel = [self createModel];
  }
  return _overflowMenuModel;
}

- (OverflowMenuModel*)createModel {
  self.openIncognitoTabAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, @"popup_menu_new_incognito_tab");
  return [[OverflowMenuModel alloc] initWithDestinations:@[]
                                                 actions:@[
                                                   self.openIncognitoTabAction,
                                                 ]];
}
@end
