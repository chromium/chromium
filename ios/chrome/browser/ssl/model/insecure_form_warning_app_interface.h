// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_MODEL_INSECURE_FORM_WARNING_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SSL_MODEL_INSECURE_FORM_WARNING_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// The app interface for HTTPS upgrade tests.
@interface InsecureFormWarningAppInterface : NSObject

+ (void)setInsecureFormPortsForTesting:(int)portTreatedAsSecure
                 portTreatedAsInsecure:(int)portTreatedAsInsecure;

@end

#endif  // IOS_CHROME_BROWSER_SSL_MODEL_INSECURE_FORM_WARNING_APP_INTERFACE_H_
