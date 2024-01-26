// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_RESPONSES_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_RESPONSES_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "ios/chrome/browser/overlays/model/public/overlay_response_info.h"

namespace save_address_profile_infobar_modal_responses {

// Response info used to create dispatched OverlayResponses once the user
// presses "Save/Update" action on the Edit Modal.
class EditedProfileSaveAction
    : public OverlayResponseInfo<EditedProfileSaveAction> {
 public:
  ~EditedProfileSaveAction() override;

  autofill::AutofillProfile* profile_data() const { return profile_data_; }

 private:
  OVERLAY_USER_DATA_SETUP(EditedProfileSaveAction);
  EditedProfileSaveAction(autofill::AutofillProfile* profileData);

  raw_ptr<autofill::AutofillProfile> profile_data_;
};

// Response info used to create dispatched OverlayResponses once the user
// cancels the modal.
class CancelViewAction : public OverlayResponseInfo<CancelViewAction> {
 public:
  ~CancelViewAction() override;

  BOOL edit_view_is_dismissed() const { return edit_view_is_dismissed_; }

 private:
  OVERLAY_USER_DATA_SETUP(CancelViewAction);
  CancelViewAction(BOOL edit_view_is_dismissed);

  BOOL edit_view_is_dismissed_;
};

// Response info used to create dispatched OverlayResponses once the user
// clicks on "No Thanks".
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(NoThanksViewAction);

}  // namespace save_address_profile_infobar_modal_responses

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_MODAL_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_RESPONSES_H_
