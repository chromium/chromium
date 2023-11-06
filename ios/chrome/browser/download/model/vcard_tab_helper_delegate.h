// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_DELEGATE_H_

// TabHelper which manages vcard files.
@protocol VcardTabHelperDelegate

// Called to open a Vcard. `data` cannot be nil.
- (void)openVcardFromData:(NSData*)vcardData;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_DELEGATE_H_
