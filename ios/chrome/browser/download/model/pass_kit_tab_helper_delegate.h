// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_PASS_KIT_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_PASS_KIT_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class PKPass;
class PassKitTabHelper;
namespace web {
class WebState;
}  // namespace web

// Delegate for PassKitTabHelper class.
@protocol PassKitTabHelperDelegate<NSObject>

// Called to present "Add pkpass" dialog. `pass` can be nil if PassKitTabHelper
// failed to download or parse pkpass file.
- (void)passKitTabHelper:(nonnull PassKitTabHelper*)tabHelper
    presentDialogForPass:(nullable PKPass*)pass
                webState:(nonnull web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_PASS_KIT_TAB_HELPER_DELEGATE_H_
