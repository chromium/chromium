// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_PROFILE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_PROFILE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Represents a profile for autofilling address forms.
CWV_EXPORT
@interface CWVAutofillProfile : NSObject

// The full name. e.g. "Homer Simpson".
@property(nonatomic, copy, nullable) NSString* name;
// The company name. e.g. "Google".
@property(nonatomic, copy, nullable) NSString* company;
// The first line of the address. e.g. "123 Main Street".
@property(nonatomic, copy, nullable) NSString* address1;
// The second line of the address. e.g. "Apt #1337".
@property(nonatomic, copy, nullable) NSString* address2;
// The city name. e.g. "Springfield".
@property(nonatomic, copy, nullable) NSString* city;
// The state name. e.g. "IL" or "Illinois".
@property(nonatomic, copy, nullable) NSString* state;
// The zipcode. e.g. "55123".
@property(nonatomic, copy, nullable) NSString* zipcode;
// The country name. e.g. "USA".
@property(nonatomic, copy, nullable) NSString* country;
// The phone number. e.g. "310-310-6000".
@property(nonatomic, copy, nullable) NSString* phone;
// The email. e.g. "hjs@aol.com".
@property(nonatomic, copy, nullable) NSString* email;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_PROFILE_H_
