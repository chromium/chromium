// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_responses.h"

namespace save_address_profile_infobar_modal_responses {

#pragma mark - EditedProfileSaveAction

OVERLAY_USER_DATA_SETUP_IMPL(EditedProfileSaveAction);

EditedProfileSaveAction::EditedProfileSaveAction(
    autofill::AutofillProfile* profileData)
    : profile_data_(profileData) {}

EditedProfileSaveAction::~EditedProfileSaveAction() = default;

#pragma mark - CancelViewAction

OVERLAY_USER_DATA_SETUP_IMPL(CancelViewAction);

CancelViewAction::CancelViewAction(BOOL edit_view_is_dismissed)
    : edit_view_is_dismissed_(edit_view_is_dismissed) {}

CancelViewAction::~CancelViewAction() = default;

#pragma mark - NoThanksViewAction

OVERLAY_USER_DATA_SETUP_IMPL(NoThanksViewAction);

}  // save_address_profile_infobar_modal_responses
