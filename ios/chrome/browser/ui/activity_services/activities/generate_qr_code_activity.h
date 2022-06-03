// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_GENERATE_QR_CODE_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_GENERATE_QR_CODE_ACTIVITY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#include "url/gurl.h"

// Activity that ends up showing a QR code for the given URL.
@interface GenerateQrCodeActivity : UIActivity

// Initializes the GenerateQrCodeActivity with the |activityURL| used to
// generate the QR code, the |title| of the page at that URL, and a |dispatcher|
// to handle the command.
- (instancetype)initWithURL:(const GURL&)activityURL
                      title:(NSString*)title
                    handler:(id<QRGenerationCommands>)handler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_GENERATE_QR_CODE_ACTIVITY_H_
