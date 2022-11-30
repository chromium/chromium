// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_CARD_INFOBAR_MODAL_OVERLAY_RESPONSES_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_CARD_INFOBAR_MODAL_OVERLAY_RESPONSES_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/overlays/public/overlay_response_info.h"
#include "url/gurl.h"

namespace save_card_infobar_overlays {

// Main action response config for Save Card that passes the information of a
// card to be saved.
class SaveCardMainAction : public OverlayResponseInfo<SaveCardMainAction> {
 public:
  ~SaveCardMainAction() override;

  // The cardholder name of the card to be saved.
  NSString* cardholder_name() const { return cardholder_name_; }
  // The expiration month of the card to be saved.
  NSString* expiration_month() const { return expiration_month_; }
  // The expiration year of the card to be saved.
  NSString* expiration_year() const { return expiration_year_; }

 private:
  OVERLAY_USER_DATA_SETUP(SaveCardMainAction);
  SaveCardMainAction(NSString* cardholder_name,
                     NSString* expiration_month,
                     NSString* expiration_year);

  NSString* cardholder_name_ = nil;
  NSString* expiration_month_ = nil;
  NSString* expiration_year_ = nil;
};

// Response config to load a URL tapped in the modal.
class SaveCardLoadURL : public OverlayResponseInfo<SaveCardLoadURL> {
 public:
  ~SaveCardLoadURL() override;

  const GURL& link_url() const { return link_url_; }

 private:
  OVERLAY_USER_DATA_SETUP(SaveCardLoadURL);
  SaveCardLoadURL(const GURL& link_url);

  GURL link_url_;
};

}  // namespace save_card_infobar_overlays
#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_SAVE_CARD_INFOBAR_MODAL_OVERLAY_RESPONSES_H_
