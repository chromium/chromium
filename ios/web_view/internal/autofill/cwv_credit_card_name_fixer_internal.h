// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_NAME_FIXER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_NAME_FIXER_INTERNAL_H_

#import "ios/web_view/public/cwv_credit_card_name_fixer.h"

#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"

@interface CWVCreditCardNameFixer ()

// Initialize with a suggested |name| and a |callback| to be invoked with the
// chosen name.
- (instancetype)initWithName:(NSString*)name
                    callback:(base::OnceCallback<void(const std::u16string&)>)
                                 callback NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CREDIT_CARD_NAME_FIXER_INTERNAL_H_
