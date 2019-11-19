// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CONTENT_INJECTOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CONTENT_INJECTOR_H_

#import <Foundation/Foundation.h>

// Protocol to send Manual Fill user selections to be filled in the active web
// state.
@protocol ManualFillContentInjector <NSObject>

// Must be called before |userDidPickContent| to validate if a value type can be
// injected, if either flag is true. If not, an alert is given to the user and
// NO is returned.
// @param passwordField YES if the user selected content that requires a
// password field to be injected.
// @param requiresHTTPS YES if the user selected a field, that requires an HTTPS
// context to be injected.
- (BOOL)canUserInjectInPasswordField:(BOOL)passwordField
                       requiresHTTPS:(BOOL)requiresHTTPS;

// Called after the user selects an element to be used as the input for the
// current form field.
//
// @param content The selected string.
// @param passwordField YES if the user selected content that requires a
// password field to be injected.
// @param requiresHTTPS YES if the user selected a field, that requires an HTTPS
// context to be injected.
- (void)userDidPickContent:(NSString*)content
             passwordField:(BOOL)passwordField
             requiresHTTPS:(BOOL)requiresHTTPS;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CONTENT_INJECTOR_H_
