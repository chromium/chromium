// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_responses.h"

#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace save_card_infobar_overlays {

#pragma mark - SaveCardMainAction

OVERLAY_USER_DATA_SETUP_IMPL(SaveCardMainAction);

SaveCardMainAction::SaveCardMainAction(NSString* cardholder_name,
                                       NSString* expiration_month,
                                       NSString* expiration_year)
    : cardholder_name_(cardholder_name),
      expiration_month_(expiration_month),
      expiration_year_(expiration_year) {}

SaveCardMainAction::~SaveCardMainAction() = default;

#pragma mark - SaveCardLoadURL

OVERLAY_USER_DATA_SETUP_IMPL(SaveCardLoadURL);

SaveCardLoadURL::SaveCardLoadURL(const GURL& link_url) : link_url_(link_url) {}

SaveCardLoadURL::~SaveCardLoadURL() = default;

}  // namespace save_card_infobar_overlays
