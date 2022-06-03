// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_

#include "base/callback.h"

class AppLauncherTabHelper;
class GURL;

// Interface for handling application launching from a tab helper.
class AppLauncherTabHelperDelegate {
 public:
  AppLauncherTabHelperDelegate() = default;
  virtual ~AppLauncherTabHelperDelegate() = default;

  // Launches application that has |URL| if possible (optionally after
  // confirming via dialog).
  virtual void LaunchAppForTabHelper(AppLauncherTabHelper* tab_helper,
                                     const GURL& url,
                                     bool link_transition) = 0;

  // Alerts the user that there have been repeated attempts to launch
  // the application. |completionHandler| is called with the user's
  // response on whether to launch the application.
  virtual void ShowRepeatedAppLaunchAlert(
      AppLauncherTabHelper* tab_helper,
      base::OnceCallback<void(bool)> completion) = 0;
};

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_
