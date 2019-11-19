// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/side_menu_items.h"

#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/app/app_delegate.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/persistence/remoting_preferences.h"

#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static NSString* const kFeedbackContext = @"SideMenuFeedbackContext";

#pragma mark - SideMenuItem

@implementation SideMenuItem

@synthesize title = _title;
@synthesize icon = _icon;
@synthesize action = _action;

- (instancetype)initWithTitle:(NSString*)title
                         icon:(UIImage*)icon
                       action:(SideMenuItemAction)action {
  _title = title;
  _icon = icon;
  _action = action;
  return self;
}

@end

#pragma mark - SideMenuItemsProvider

@implementation SideMenuItemsProvider

+ (NSArray<NSArray<SideMenuItem*>*>*)sideMenuItems {
  static NSArray<NSArray<SideMenuItem*>*>* items = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    items = @[
#if !defined(NDEBUG)
      @[
        [[SideMenuItem alloc]
            initWithTitle:@"Toggle WebRTC"
                     icon:RemotingTheme.settingsIcon
                   action:^{
                     BOOL newValue = ![RemotingPreferences.instance
                         boolForFlag:RemotingFlagUseWebRTC];
                     [RemotingPreferences.instance
                         setBool:newValue
                         forFlag:RemotingFlagUseWebRTC];
                     [RemotingPreferences.instance synchronizeFlags];
                     NSString* message =
                         [NSString stringWithFormat:@"Using WebRTC: %s",
                                                    newValue ? "Yes" : "No"];
                     [MDCSnackbarManager
                         showMessage:[MDCSnackbarMessage
                                         messageWithText:message]];
                   }],
      ],
#endif  // !defined(NDEBUG)
      @[
        [[SideMenuItem alloc]
            initWithTitle:l10n_util::GetNSString(IDS_ACTIONBAR_SEND_FEEDBACK)
                     icon:RemotingTheme.feedbackIcon
                   action:^{
                     [AppDelegate.instance
                         presentFeedbackFlowWithContext:kFeedbackContext];
                   }],
        [[SideMenuItem alloc]
            initWithTitle:l10n_util::GetNSString(IDS_ACTIONBAR_HELP)
                     icon:RemotingTheme.helpIcon
                   action:^{
                     [AppDelegate.instance presentHelpCenter];
                   }],
      ],
      @[
        [[SideMenuItem alloc]
            initWithTitle:l10n_util::GetNSString(IDS_TERMS_OF_SERVICE)
                     icon:nil
                   action:^{
                     [AppDelegate.instance presentTermsOfService];
                   }],
        [[SideMenuItem alloc]
            initWithTitle:l10n_util::GetNSString(IDS_PRIVACY_POLICY)
                     icon:nil
                   action:^{
                     [AppDelegate.instance presentPrivacyPolicy];
                   }]
      ]
    ];
  });
  return items;
}

@end
