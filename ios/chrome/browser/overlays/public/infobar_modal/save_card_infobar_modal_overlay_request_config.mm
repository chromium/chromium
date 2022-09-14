// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_request_config.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/autofill/save_card_message_with_links.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace save_card_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(SaveCardModalRequestConfig);

SaveCardModalRequestConfig::SaveCardModalRequestConfig(InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  autofill::AutofillSaveCardInfoBarDelegateMobile* delegate =
      static_cast<autofill::AutofillSaveCardInfoBarDelegateMobile*>(
          infobar_->delegate());

  cardholder_name_ = delegate->cardholder_name();
  expiration_date_month_ = delegate->expiration_date_month();
  expiration_date_year_ = delegate->expiration_date_year();
  card_last_four_digits_ = delegate->card_last_four_digits();
  issuer_icon_id_ = delegate->issuer_icon_id();
  legal_message_lines_ = LegalMessagesForModal(delegate);
  current_card_saved_ = infobar->accepted();
  should_upload_credentials_ = delegate->upload();
  displayed_target_account_email_ = delegate->displayed_target_account_email();
  displayed_target_account_avatar_ =
      delegate->displayed_target_account_avatar();
}

SaveCardModalRequestConfig::~SaveCardModalRequestConfig() = default;

void SaveCardModalRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}

NSMutableArray<SaveCardMessageWithLinks*>*
SaveCardModalRequestConfig::LegalMessagesForModal(
    autofill::AutofillSaveCardInfoBarDelegateMobile* delegate) {
  NSMutableArray<SaveCardMessageWithLinks*>* legalMessages =
      [[NSMutableArray alloc] init];
  // Only display legal Messages if the card is being uploaded and there are
  // any.
  if (delegate->upload() && !delegate->legal_message_lines().empty()) {
    for (const auto& line : delegate->legal_message_lines()) {
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
  }
  return legalMessages;
}

}  // namespace save_card_infobar_overlays
