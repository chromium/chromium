// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace password_manager {

NSString* const kPasswordCheckupHeaderImageGreen =
    @"password_checkup_header_green";

NSString* const kPasswordCheckupHeaderImageLoading =
    @"password_checkup_header_loading";

NSString* const kPasswordCheckupHeaderImageRed = @"password_checkup_header_red";

NSString* const kPasswordCheckupHeaderImageYellow =
    @"password_checkup_header_yellow";

const char kPasswordManagerHelpCenterChangeUnsafePasswordsURL[] =
    "https://support.google.com/accounts/answer/9457609";

const char kPasswordManagerHelpCenterCreateStrongPasswordsURL[] =
    "https://support.google.com/accounts/answer/32040";

}  // namespace password_manager
