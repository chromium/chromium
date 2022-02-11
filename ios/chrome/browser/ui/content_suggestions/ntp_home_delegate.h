// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_DELEGATE_H_

// Handles NTP Home model delegation.
@protocol NTPHomeDelegate <NSObject>

// Signals to the delegate to reload its content.
- (void)reload;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_DELEGATE_H_
