// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_DELEGATE_H_
#define IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_DELEGATE_H_

#import <Foundation/Foundation.h>

@class ShareExtensionSheet;

// Delegate protocol for `ShareExtensionSheet`.
@protocol ShareExtensionDelegate

- (void)didTapCloseShareExtensionSheet:
    (ShareExtensionSheet*)shareExtensionSheetSheet;
- (void)didTapOpenInChromeShareExtensionSheet:
    (ShareExtensionSheet*)shareExtensionSheetSheet;
- (void)didTapMoreOptionsShareExtensionSheet:
    (ShareExtensionSheet*)shareExtensionSheetSheet;

@end

#endif  // IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_DELEGATE_H_
