// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
#define IOS_CHROME_BROWSER_CHROME_SWITCHES_H_

// Defines all the command-line switches used by iOS Chrome.

namespace switches {

extern const char kDisableEnterprisePolicy[];
extern const char kDisableIOSPasswordSuggestions[];
extern const char kDisableThirdPartyKeyboardWorkaround[];

extern const char kEnableEnterprisePolicy[];
extern const char kEnableIOSHandoffToOtherDevices[];
extern const char kEnableSpotlightActions[];
extern const char kEnableThirdPartyKeyboardWorkaround[];
extern const char kInstallManagedBookmarksHandler[];
extern const char kInstallURLBlocklistHandlers[];

extern const char kUserAgent[];

}  // namespace switches

#endif  // IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
