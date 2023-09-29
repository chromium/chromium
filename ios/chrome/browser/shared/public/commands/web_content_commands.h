// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WEB_CONTENT_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WEB_CONTENT_COMMANDS_H_

@class PKPass;

// Commands for starting UI in response to certain types of content loading.
@protocol WebContentCommands

// Opens StoreKit modal to present a product using `productParameters`.
// SKStoreProductParameterITunesItemIdentifier key must be set in
// `productParameters`.
- (void)showAppStoreWithParameters:(NSDictionary*)productParameters;

// Opens the system PassKit dialog to add `passes`.
- (void)showDialogForPassKitPasses:(NSArray<PKPass*>*)passes;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WEB_CONTENT_COMMANDS_H_
