// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_ADDRESS_FIELD_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_ADDRESS_FIELD_H_

#import <Foundation/Foundation.h>

// Data object used to store the info about the address field asking for user
// input.
@interface AutofillProfileAddressField : NSObject
@property(nonatomic, copy) NSString* fieldLabel;
@property(nonatomic, copy) NSString* fieldType;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_ADDRESS_FIELD_H_
