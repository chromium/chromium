// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_QR_GENERATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_QR_GENERATION_COMMANDS_H_

#import "ios/chrome/browser/shared/public/commands/generate_qr_code_command.h"

// QRGenerationCommands contains commands related to generating QR codes.
@protocol QRGenerationCommands <NSObject>

// Generates a QR code based on the `command` properties and displays it.
- (void)generateQRCode:(GenerateQRCodeCommand*)command;

// Dismisses the view showing a QR code, if present.
- (void)hideQRCode;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_QR_GENERATION_COMMANDS_H_
