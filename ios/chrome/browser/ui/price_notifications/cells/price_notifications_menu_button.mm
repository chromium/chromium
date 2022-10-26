// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_menu_button.h"

#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The length of the menu button.
const CGFloat kMenuButtonLength = 44;
// The size of the menu button's symbol image.
const CGFloat kMenuSymbolPointSize = 17;
}  // namespace

@implementation PriceNotificationsMenuButton

- (instancetype)init {
  self = [super init];
  if (self) {
    self.accessibilityIdentifier =
        kPriceNotificationsListItemMenuButtonIdentifier;
    self.showsMenuAsPrimaryAction = YES;
    self.backgroundColor = [UIColor colorNamed:kGrey100Color];
    self.layer.cornerRadius = kMenuButtonLength / 2;
    self.clipsToBounds = YES;

    self.tintColor = [UIColor colorNamed:kBlueColor];
    [self setImage:DefaultSymbolTemplateWithPointSize(kMenuSymbol,
                                                      kMenuSymbolPointSize)
          forState:UIControlStateNormal];

    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor constraintEqualToConstant:kMenuButtonLength],
      [self.widthAnchor constraintEqualToConstant:kMenuButtonLength]
    ]];
  }
  return self;
}

@end
