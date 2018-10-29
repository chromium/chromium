// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_DELEGATE_H_

class NewTabPageTabHelper;

// Protocol defining a delegate for the NTP tab helper.
@protocol NewTabPageTabHelperDelegate
// Tells the delegate the NTP needs to be shown or hidden.
- (void)newTabPageHelperDidChangeVisibility:(NewTabPageTabHelper*)NTPHelper
                                forWebState:(web::WebState*)webState;
@end

#endif  // IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_DELEGATE_H_
