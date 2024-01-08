// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_

#include "base/functional/callback.h"

class AppLauncherTabHelper;
class GURL;

// The reason why the AppLauncherTabHelperDelegate wants to show a user prompt.
enum class AppLauncherAlertCause {
  kOther,
  kRepeatedLaunchDetected,
  kOpenFromIncognito,
  kNoUserInteraction,
  kAppLaunchFailed,
};

// Interface for handling application launching from a tab helper.
class AppLauncherTabHelperDelegate {
 public:
  AppLauncherTabHelperDelegate() = default;
  virtual ~AppLauncherTabHelperDelegate() = default;

  // Launches application that has `url` if possible (optionally after
  // confirming via dialog). Completion is called with a boolean indicating if
  // the opening was successful.
  // `completion` is called when the external application is launched. The
  // parameter indicates if the launch was successful.
  // If the launch was successful, `back_to_app_completion` is called on next
  // scene activation.
  virtual void LaunchAppForTabHelper(
      AppLauncherTabHelper* tab_helper,
      const GURL& url,
      base::OnceCallback<void(bool)> completion,
      base::OnceCallback<void()> back_to_app_completion) = 0;

  // Alerts the user that there have been repeated attempts to launch
  // the application. `completion` is called with the user's response on whether
  // to launch the application.
  virtual void ShowAppLaunchAlert(
      AppLauncherTabHelper* tab_helper,
      AppLauncherAlertCause cause,
      base::OnceCallback<void(bool)> completion) = 0;
};

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_
