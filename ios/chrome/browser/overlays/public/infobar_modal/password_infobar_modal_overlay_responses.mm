// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_responses.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace password_infobar_modal_responses {

#pragma mark - UpdateCredentialsInfo

OVERLAY_USER_DATA_SETUP_IMPL(UpdateCredentialsInfo);

UpdateCredentialsInfo::UpdateCredentialsInfo(NSString* username,
                                             NSString* password)
    : username_([username copy]), password_([password copy]) {}

UpdateCredentialsInfo::~UpdateCredentialsInfo() = default;

#pragma mark - NeverSaveCredentials

OVERLAY_USER_DATA_SETUP_IMPL(NeverSaveCredentials);

#pragma mark - PresentPasswordSettings

OVERLAY_USER_DATA_SETUP_IMPL(PresentPasswordSettings);

}  // password_infobar_modal_responses
