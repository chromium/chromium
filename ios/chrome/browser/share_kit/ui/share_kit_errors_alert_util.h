// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_UI_SHARE_KIT_ERRORS_ALERT_UTIL_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_UI_SHARE_KIT_ERRORS_ALERT_UTIL_H_

#import <UIKit/UIKit.h>

namespace share_kit {

// Returns the alert displayed when the shared link is invalid.
UIAlertController* InvalidLinkAlert();

}  // namespace share_kit

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_UI_SHARE_KIT_ERRORS_ALERT_UTIL_H_
