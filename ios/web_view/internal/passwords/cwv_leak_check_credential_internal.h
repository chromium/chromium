// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_LEAK_CHECK_CREDENTIAL_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_LEAK_CHECK_CREDENTIAL_INTERNAL_H_

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#import "ios/web_view/public/cwv_leak_check_credential.h"

@interface CWVLeakCheckCredential ()

// Initializer to wrap the provided credential. User data associated with the
// provided credential will not be passed to the leak check service.
- (instancetype)initWithCredential:
    (std::unique_ptr<password_manager::LeakCheckCredential>)credential
    NS_DESIGNATED_INITIALIZER;

// The internal credential we wrap. This is intentionally not declared as a
// property to hide it from [NSObject valueForKey:]
- (const password_manager::LeakCheckCredential&)internalCredential;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_LEAK_CHECK_CREDENTIAL_INTERNAL_H_
