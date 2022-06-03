// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_GENERATE_QR_CODE_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_GENERATE_QR_CODE_COMMAND_H_

#import <Foundation/Foundation.h>

#include "url/gurl.h"

// Command sent to generate a QR code for a given URL.
@interface GenerateQRCodeCommand : NSObject

// Initializes a command containing the URL to generate a QR code for along with
// that URL's page title.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title NS_DESIGNATED_INITIALIZER;

// Mark inherited initializer as unavailable to prevent calling it by mistake.
- (instancetype)init NS_UNAVAILABLE;

// URL for the QR code.
@property(nonatomic, readonly) const GURL& URL;

// Title of the page, which can be used for display purposes.
@property(copy, nonatomic, readonly) NSString* title;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_GENERATE_QR_CODE_COMMAND_H_