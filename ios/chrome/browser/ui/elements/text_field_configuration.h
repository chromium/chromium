// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ELEMENTS_TEXT_FIELD_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_ELEMENTS_TEXT_FIELD_CONFIGURATION_H_

#import <Foundation/Foundation.h>

// Holds the configurable options for a UITextField.
//
// The properties here match the ones of UITextField. Find the respective
// documentation of each in UITextField.h.
@interface TextFieldConfiguration : NSObject

@property(nonatomic, copy, readonly) NSString* text;
@property(nonatomic, copy, readonly) NSString* placeholder;
@property(nonatomic, copy, readonly) NSString* accessibilityIdentifier;
@property(nonatomic, readonly, getter=isSecureTextEntry) BOOL secureTextEntry;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithText:(NSString*)text
                 placeholder:(NSString*)placeholder
     accessibilityIdentifier:(NSString*)accessibilityIdentifier
             secureTextEntry:(BOOL)secureTextEntry NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_ELEMENTS_TEXT_FIELD_CONFIGURATION_H_
