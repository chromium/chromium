// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_util.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysUTF16ToNSString;
using base::UTF8ToUTF16;

NSString* RecordIdentifierForPasswordForm(
    const password_manager::PasswordForm& form) {
  // These are the UNIQUE keys in the login database.
  return SysUTF16ToNSString(UTF8ToUTF16(form.url.spec() + "|") +
                            form.username_element + u"|" + form.username_value +
                            u"|" + form.password_element +
                            UTF8ToUTF16("|" + form.signon_realm));
}
