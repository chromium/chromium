// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/save_card_infobar_controller.h"

#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/infobar_controller+protected.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/ui/autofill/save_card_infobar_view.h"
#import "ios/chrome/browser/ui/autofill/save_card_infobar_view_delegate.h"
#import "ios/chrome/browser/ui/autofill/save_card_message_with_links.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSaveCardInfobarViewLocalAccessibilityID =
    @"SaveCardInfobarViewLocalAccessibilityID";
NSString* const kSaveCardInfobarViewUploadAccessibilityID =
    @"SaveCardInfobarViewUploadAccessibilityID";

namespace {

// Returns the image for the infobar close button.
UIImage* InfoBarCloseImage() {
  ui::ResourceBundle& resourceBundle = ui::ResourceBundle::GetSharedInstance();
  return resourceBundle.GetNativeImageNamed(IDR_IOS_INFOBAR_CLOSE).ToUIImage();
}

// Returns the title for the given infobar button.
base::string16 GetTitleForButton(ConfirmInfoBarDelegate* delegate,
                                 ConfirmInfoBarDelegate::InfoBarButton button) {
  return (delegate->GetButtons() & button) ? delegate->GetButtonLabel(button)
                                           : base::string16();
}

}  // namespace

#pragma mark - SaveCardInfoBarController

@interface SaveCardInfoBarController ()<SaveCardInfoBarViewDelegate>

// Overrides superclass property.
@property(nonatomic, assign)
    autofill::AutofillSaveCardInfoBarDelegateMobile* infoBarDelegate;

@property(nonatomic, weak) SaveCardInfoBarView* infoBarView;

@end

@implementation SaveCardInfoBarController

@dynamic infoBarDelegate;

- (instancetype)initWithInfoBarDelegate:
    (autofill::AutofillSaveCardInfoBarDelegateMobile*)infoBarDelegate {
  return [super initWithInfoBarDelegate:infoBarDelegate];
}

- (UIView*)infobarView {
  SaveCardInfoBarView* infoBarView =
      [[SaveCardInfoBarView alloc] initWithFrame:CGRectZero];
  self.infoBarView = infoBarView;
  self.infoBarView.accessibilityIdentifier =
      self.infoBarDelegate->upload() ? kSaveCardInfobarViewUploadAccessibilityID
                                     : kSaveCardInfobarViewLocalAccessibilityID;
  self.infoBarView.delegate = self;

  // Close button.
  [self.infoBarView setCloseButtonImage:InfoBarCloseImage()];

  // Icon.
  gfx::Image icon = self.infoBarDelegate->GetIcon();
  DCHECK(!icon.IsEmpty());
  if (self.infoBarDelegate->IsGooglePayBrandingEnabled()) {
    [self.infoBarView setGooglePayIcon:icon.ToUIImage()];
  } else {
    UIImage* iconImage = [icon.ToUIImage()
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [self.infoBarView setIcon:iconImage];
  }

  // Message, if any.
  base::string16 messageText = self.infoBarDelegate->GetMessageText();
  if (!messageText.empty()) {
    SaveCardMessageWithLinks* message = [[SaveCardMessageWithLinks alloc] init];
    const base::string16 linkText = self.infoBarDelegate->GetLinkText();
    GURL linkURL = self.infoBarDelegate->GetLinkURL();

    if (!linkText.empty() && !linkURL.is_empty()) {
      std::vector<GURL> linkURLs;
      linkURLs.push_back(linkURL);
      message.linkURLs = linkURLs;
      message.linkRanges = [[NSArray alloc]
          initWithObjects:[NSValue valueWithRange:NSMakeRange(
                                                      messageText.length() + 1,
                                                      linkText.length())],
                          nil];
      // Append the link text to the message.
      messageText += base::UTF8ToUTF16(" ") + linkText;
    }
    message.messageText = base::SysUTF16ToNSString(messageText);
    [self.infoBarView setMessage:message];
  }

  // Description, if any.
  const base::string16 description = self.infoBarDelegate->GetDescriptionText();
  if (!description.empty()) {
    [self.infoBarView setDescription:base::SysUTF16ToNSString(description)];
  }

  // Card details.
  [self.infoBarView
      setCardIssuerIcon:NativeImage(self.infoBarDelegate->issuer_icon_id())];
  [self.infoBarView setCardLabel:base::SysUTF16ToNSString(
                                     self.infoBarDelegate->card_label())];
  [self.infoBarView
      setCardSublabel:base::SysUTF16ToNSString(
                          self.infoBarDelegate->card_sub_label())];

  // Legal messages, if any.
  if (!self.infoBarDelegate->legal_message_lines().empty()) {
    NSMutableArray* legalMessages = [[NSMutableArray alloc] init];
    for (const auto& line : self.infoBarDelegate->legal_message_lines()) {
      SaveCardMessageWithLinks* message =
          [[SaveCardMessageWithLinks alloc] init];
      message.messageText = base::SysUTF16ToNSString(line.text());
      NSMutableArray* linkRanges = [[NSMutableArray alloc] init];
      std::vector<GURL> linkURLs;
      for (const auto& link : line.links()) {
        [linkRanges addObject:[NSValue valueWithRange:link.range.ToNSRange()]];
        linkURLs.push_back(link.url);
      }
      message.linkRanges = linkRanges;
      message.linkURLs = linkURLs;
      [legalMessages addObject:message];
    }
    [self.infoBarView setLegalMessages:legalMessages];
  }

  // Cancel button.
  const base::string16 cancelButtonTitle = GetTitleForButton(
      self.infoBarDelegate, ConfirmInfoBarDelegate::BUTTON_CANCEL);
  [self.infoBarView
      setCancelButtonTitle:base::SysUTF16ToNSString(cancelButtonTitle)];

  // Confirm button.
  const base::string16 confirmButtonTitle = GetTitleForButton(
      self.infoBarDelegate, ConfirmInfoBarDelegate::BUTTON_OK);
  [self.infoBarView
      setConfirmButtonTitle:base::SysUTF16ToNSString(confirmButtonTitle)];

  return infoBarView;
}

#pragma mark - SaveCardInfoBarViewDelegate

- (void)saveCardInfoBarViewDidTapLink:(SaveCardInfoBarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.infoBarDelegate->LinkClicked(WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

- (void)saveCardInfoBarView:(SaveCardInfoBarView*)sender
         didTapLegalLinkURL:(const GURL&)linkURL {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.infoBarDelegate->OnLegalMessageLinkClicked(linkURL);
}

- (void)saveCardInfoBarViewDidTapClose:(SaveCardInfoBarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  self.infoBarDelegate->InfoBarDismissed();
  self.delegate->RemoveInfoBar();
}

- (void)saveCardInfoBarViewDidTapCancel:(SaveCardInfoBarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  if (self.infoBarDelegate->Cancel()) {
    self.delegate->RemoveInfoBar();
  }
}

- (void)saveCardInfoBarViewDidTapConfirm:(SaveCardInfoBarView*)sender {
  if ([self shouldIgnoreUserInteraction])
    return;

  if (self.infoBarDelegate->Accept()) {
    self.delegate->RemoveInfoBar();
  }
}

@end
