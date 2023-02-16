// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_handler.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/ui/link_to_text/link_to_text_mediator.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserEditMenuHandler ()
@end

@implementation BrowserEditMenuHandler

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  if (!base::FeatureList::IsEnabled(kIOSCustomBrowserEditMenu)) {
    return;
  }
  NSString* title = l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT);
  NSString* linkToTextId = @"chromecommand.linktotext";
  UICommand* menuCommand = [UICommand commandWithTitle:title
                                                 image:nil
                                                action:@selector(linkToText:)
                                          propertyList:linkToTextId];

  UIMenu* linkToTextMenu = [UIMenu menuWithTitle:title
                                           image:nil
                                      identifier:linkToTextId
                                         options:UIMenuOptionsDisplayInline
                                        children:@[ menuCommand ]];
  [builder insertChildMenu:linkToTextMenu atEndOfMenuForIdentifier:UIMenuRoot];
}

- (void)addEditMenuEntries {
  if (base::FeatureList::IsEnabled(kIOSCustomBrowserEditMenu)) {
    return;
  }
  if (!base::FeatureList::IsEnabled(kSharedHighlightingIOS)) {
    return;
  }
  NSString* title = l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT);
  UIMenuItem* menuItem =
      [[UIMenuItem alloc] initWithTitle:title action:@selector(linkToText:)];
  RegisterEditMenuItem(menuItem);
}

- (BOOL)canPerformChromeAction:(SEL)action withSender:(id)sender {
  if (action == @selector(linkToText:)) {
    return [self.linkToTextDelegate shouldOfferLinkToText];
  }
  return NO;
}

- (void)linkToText:(UIMenuItem*)item {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  DCHECK(self.linkToTextDelegate);
  [self.linkToTextDelegate handleLinkToTextSelection];
}

@end
