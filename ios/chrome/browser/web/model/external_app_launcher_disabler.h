// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_EXTERNAL_APP_LAUNCHER_DISABLER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_EXTERNAL_APP_LAUNCHER_DISABLER_H_

#include "ios/chrome/browser/web/model/external_app_launcher.h"

// An override of ExternalAppLauncher to disable launching external
// apps for test purposes.
@interface ExternalAppLauncherDisabler : ExternalAppLauncher

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_EXTERNAL_APP_LAUNCHER_DISABLER_H_
