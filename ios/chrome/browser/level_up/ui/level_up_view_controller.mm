// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_view_controller.h"

#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation LevelUpViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_LEVEL_UP);

  UIButton* menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
  menuButton.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  menuButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [menuButton setImage:DefaultSymbolTemplateWithPointSize(
                           kEllipsisSymbol, kSymbolAccessoryPointSize)
              forState:UIControlStateNormal];

  menuButton.menu = [UIMenu menuWithTitle:@"" children:@[]];
  menuButton.showsMenuAsPrimaryAction = YES;
  self.navigationItem.leftBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:menuButton];

  UIButton* dismissButton = [UIButton buttonWithType:UIButtonTypeSystem];
  dismissButton.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  dismissButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [dismissButton setImage:DefaultSymbolTemplateWithPointSize(
                              kXMarkSymbol, kSymbolAccessoryPointSize)
                 forState:UIControlStateNormal];

  [dismissButton addTarget:self
                    action:@selector(dismiss)
          forControlEvents:UIControlEventTouchUpInside];
  self.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:dismissButton];
}

#pragma mark - Private

- (void)dismiss {
  [self.handler dismissLevelUp];
}

@end
