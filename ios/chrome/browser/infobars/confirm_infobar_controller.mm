// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/confirm_infobar_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_controller+protected.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/ui/infobars/confirm_infobar_view.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// UI Tags for the infobar elements.
typedef NS_ENUM(NSInteger, ConfirmInfoBarUITags) {
  OK = 1,
  CANCEL,
  CLOSE,
  TITLE_LINK
};

}  // namespace

#pragma mark - ConfirmInfoBarController

@interface ConfirmInfoBarController ()

// Overrides superclass property.
@property(nonatomic, readonly) ConfirmInfoBarDelegate* infoBarDelegate;

@property(nonatomic, weak) ConfirmInfoBarView* infoBarView;

@end

@implementation ConfirmInfoBarController

@dynamic infoBarDelegate;
@synthesize infoBarView = _infoBarView;

#pragma mark -
#pragma mark InfoBarController

- (instancetype)initWithInfoBarDelegate:
    (ConfirmInfoBarDelegate*)infoBarDelegate {
  return [super initWithInfoBarDelegate:infoBarDelegate];
}

- (UIView*)infobarView {
  ConfirmInfoBarView* infoBarView =
      [[ConfirmInfoBarView alloc] initWithFrame:CGRectZero];
  _infoBarView = infoBarView;
  // Model data.
  gfx::Image modelIcon = self.infoBarDelegate->GetIcon();
  int buttons = self.infoBarDelegate->GetButtons();
  NSString* buttonOK = nil;
  if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
    buttonOK = base::SysUTF16ToNSString(self.infoBarDelegate->GetButtonLabel(
        ConfirmInfoBarDelegate::BUTTON_OK));
  }
  NSString* buttonCancel = nil;
  if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    buttonCancel =
        base::SysUTF16ToNSString(self.infoBarDelegate->GetButtonLabel(
            ConfirmInfoBarDelegate::BUTTON_CANCEL));
  }

  [infoBarView addCloseButtonWithTag:ConfirmInfoBarUITags::CLOSE
                              target:self
                              action:@selector(infoBarButtonDidPress:)];

  // Optional left icon.
  if (!modelIcon.IsEmpty())
    [infoBarView addLeftIcon:modelIcon.ToUIImage()];

  // Optional message.
  [self updateInfobarLabel:infoBarView];

  if (buttonOK && buttonCancel) {
    [infoBarView addButton1:buttonOK
                       tag1:ConfirmInfoBarUITags::OK
                    button2:buttonCancel
                       tag2:ConfirmInfoBarUITags::CANCEL
                     target:self
                     action:@selector(infoBarButtonDidPress:)];
  } else if (buttonOK) {
    [infoBarView addButton:buttonOK
                       tag:ConfirmInfoBarUITags::OK
                    target:self
                    action:@selector(infoBarButtonDidPress:)];
  } else {
    // No buttons, only message.
    DCHECK(!self.infoBarDelegate->GetMessageText().empty() && !buttonCancel);
  }
  return infoBarView;
}

- (void)updateInfobarLabel:(ConfirmInfoBarView*)view {
  if (!self.infoBarDelegate->GetMessageText().length())
    return;
  if (self.infoBarDelegate->GetLinkText().length()) {
    base::string16 msgLink = base::SysNSStringToUTF16([[view class]
        stringAsLink:base::SysUTF16ToNSString(
                         self.infoBarDelegate->GetLinkText())
                 tag:ConfirmInfoBarUITags::TITLE_LINK]);
    base::string16 messageText = self.infoBarDelegate->GetMessageText();
    base::ReplaceFirstSubstringAfterOffset(
        &messageText, 0, self.infoBarDelegate->GetLinkText(), msgLink);

    __weak ConfirmInfoBarController* weakSelf = self;
    [view addLabel:base::SysUTF16ToNSString(messageText)
            action:^(NSUInteger tag) {
              [weakSelf infobarLinkDidPress:tag];
            }];
  } else {
    NSString* label =
        base::SysUTF16ToNSString(self.infoBarDelegate->GetMessageText());
    [view addLabel:label];
  }
}

#pragma mark - Handling of User Events

- (void)infoBarButtonDidPress:(id)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  NSUInteger buttonId = base::mac::ObjCCastStrict<UIButton>(sender).tag;
  switch (buttonId) {
    case ConfirmInfoBarUITags::OK:
      if (self.infoBarDelegate->Accept()) {
        self.delegate->RemoveInfoBar();
      }
      break;
    case ConfirmInfoBarUITags::CANCEL:
      if (self.infoBarDelegate->Cancel()) {
        self.delegate->RemoveInfoBar();
      }
      break;
    case ConfirmInfoBarUITags::CLOSE:
      self.infoBarDelegate->InfoBarDismissed();
      self.delegate->RemoveInfoBar();
      break;
    default:
      NOTREACHED() << "Unexpected button pressed";
      break;
  }
}

// Title link was clicked.
- (void)infobarLinkDidPress:(NSUInteger)tag {
  if ([self shouldIgnoreUserInteraction])
    return;

  DCHECK(tag == ConfirmInfoBarUITags::TITLE_LINK);
  if (self.infoBarDelegate->LinkClicked(
          WindowOpenDisposition::NEW_FOREGROUND_TAB)) {
    self.delegate->RemoveInfoBar();
  }
}

@end
