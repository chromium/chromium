// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_handler.h"

#import "base/feature_list.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/link_to_text/link_to_text_delegate.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_delegate.h"
#import "ios/chrome/browser/ui/search_with/search_with_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserEditMenuHandler ()

// A cache for the first responder that is reset on the next runloop.
@property(nonatomic, weak) UIResponder* firstResponder;

@end

@implementation BrowserEditMenuHandler {
  // Keep the original translate command to display partial translate in the
  // same conditions.
  UICommand* _originalTranslateCommand;
}

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  if (!base::FeatureList::IsEnabled(kIOSCustomBrowserEditMenu)) {
    return;
  }
  [self addLinkToText:builder];
  [self addPartialTranslate:builder];
  [self.searchWithDelegate buildMenuWithBuilder:builder];
}

- (void)addLinkToText:(id<UIMenuBuilder>)builder {
  NSString* title = l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT);
  NSString* linkToTextId = @"chromecommand.linktotext";
  UICommand* menuCommand = [UICommand
      commandWithTitle:title
                 image:DefaultSymbolWithPointSize(kHighlighterSymbol,
                                                  kSymbolActionPointSize)
                action:@selector(linkToText:)
          propertyList:linkToTextId];

  UIMenu* linkToTextMenu = [UIMenu menuWithTitle:@""
                                           image:nil
                                      identifier:linkToTextId
                                         options:UIMenuOptionsDisplayInline
                                        children:@[ menuCommand ]];
  [builder insertChildMenu:linkToTextMenu atEndOfMenuForIdentifier:UIMenuRoot];
}

- (void)addPartialTranslate:(id<UIMenuBuilder>)builder {
  if (![self.partialTranslateDelegate shouldInstallPartialTranslate]) {
    return;
  }
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_PARTIAL_TRANSLATE_EDIT_MENU_ENTRY);
  NSString* partialTranslateId = @"chromecommand.partialTranslate";
  UICommand* partialTranslateCommand =
      [UICommand commandWithTitle:title
                            image:CustomSymbolWithPointSize(
                                      kTranslateSymbol, kSymbolActionPointSize)
                           action:@selector(chromePartialTranslate:)
                     propertyList:partialTranslateId];

  // Translate command is in the lookup menu.
  // Retrieve the menu so it can be replaced with partial translate.
  UIMenu* lookupMenu = [builder menuForIdentifier:UIMenuLookup];
  NSArray* children = lookupMenu.children;
  NSInteger translateIndex = -1;
  for (NSUInteger index = 0; index < children.count; index++) {
    UIMenuElement* element = children[index];
    // Translate is a command.
    if (![element isKindOfClass:[UICommand class]]) {
      continue;
    }
    UICommand* command = base::mac::ObjCCast<UICommand>(element);
    if (command.action != NSSelectorFromString(@"_translate:")) {
      continue;
    }
    _originalTranslateCommand = command;
    translateIndex = index;
    break;
  }

  if (translateIndex == -1) {
    // Translate command not found. Fallback adding the partial translate before
    // the lookup menu.
    // TODO(crbug.com/1417639): Catch this so it can be fixed.
    UIMenu* partialTranslateMenu =
        [UIMenu menuWithTitle:title
                        image:nil
                   identifier:partialTranslateId
                      options:UIMenuOptionsDisplayInline
                     children:@[ partialTranslateCommand ]];
    [builder insertSiblingMenu:partialTranslateMenu
        beforeMenuForIdentifier:UIMenuLookup];
    return;
  }

  // Rebuild the lookup menu with partial translate
  NSMutableArray* newChildren = [NSMutableArray arrayWithArray:children];
  newChildren[translateIndex] = partialTranslateCommand;
  UIMenu* newPartialTranslate = [UIMenu menuWithTitle:lookupMenu.title
                                                image:lookupMenu.image
                                           identifier:lookupMenu.identifier
                                              options:lookupMenu.options
                                             children:newChildren];

  [builder replaceMenuForIdentifier:UIMenuLookup withMenu:newPartialTranslate];
}

- (void)addEditMenuEntries {
  if (base::FeatureList::IsEnabled(kIOSCustomBrowserEditMenu)) {
    return;
  }
  if (base::FeatureList::IsEnabled(kSharedHighlightingIOS)) {
    NSString* title = l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT);
    UIMenuItem* menuItem =
        [[UIMenuItem alloc] initWithTitle:title action:@selector(linkToText:)];
    RegisterEditMenuItem(menuItem);
  }
  if ([self.partialTranslateDelegate shouldInstallPartialTranslate]) {
    NSString* title =
        l10n_util::GetNSString(IDS_IOS_PARTIAL_TRANSLATE_EDIT_MENU_ENTRY);
    UIMenuItem* menuItem =
        [[UIMenuItem alloc] initWithTitle:title
                                   action:@selector(chromePartialTranslate:)];
    RegisterEditMenuItem(menuItem);
  }
}

- (BOOL)canPerformChromeAction:(SEL)action withSender:(id)sender {
  if (action == @selector(linkToText:)) {
    return [self.linkToTextDelegate shouldOfferLinkToText];
  }
  if (action == @selector(chromePartialTranslate:)) {
    BOOL canHandlePartialTranslate =
        [self.partialTranslateDelegate canHandlePartialTranslateSelection];
    if (canHandlePartialTranslate && [self firstResponder] &&
        _originalTranslateCommand) {
      return [[self firstResponder]
          canPerformAction:_originalTranslateCommand.action
                withSender:_originalTranslateCommand];
    }
    return canHandlePartialTranslate;
  }
  return NO;
}

#pragma mark - LinkToTextDelegate methods

- (void)linkToText:(UIMenuItem*)item {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  DCHECK(self.linkToTextDelegate);
  [self.linkToTextDelegate handleLinkToTextSelection];
}

#pragma mark - PartialTranslateDelegate methods

- (void)chromePartialTranslate:(UIMenuItem*)item {
  DCHECK([self.partialTranslateDelegate shouldInstallPartialTranslate]);
  DCHECK(self.partialTranslateDelegate);
  [self.partialTranslateDelegate handlePartialTranslateSelection];
}

#pragma mark - private methods

- (UIResponder*)firstResponder {
  if (_firstResponder) {
    return _firstResponder;
  }
  _firstResponder = GetFirstResponderSubview(self.rootView);
  __weak BrowserEditMenuHandler* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    weakSelf.firstResponder = nil;
  });
  return _firstResponder;
}

@end
