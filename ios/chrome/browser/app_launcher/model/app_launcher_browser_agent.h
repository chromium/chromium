// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tab_helper_delegate_installer.h"

@class AppLauncherSceneStateObserver;
class OverlayRequestQueue;

// A browser agent that manages opening external apps for navigations that occur
// within one of the Browser's WebStates.
class AppLauncherBrowserAgent
    : public BrowserObserver,
      public BrowserUserData<AppLauncherBrowserAgent> {
 public:
  ~AppLauncherBrowserAgent() override;

  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

 private:
  friend class BrowserUserData<AppLauncherBrowserAgent>;
  explicit AppLauncherBrowserAgent(Browser* browser);

  // Helper object that handles delegated AppLauncherTabHelper functionality.
  class TabHelperDelegate : public AppLauncherTabHelperDelegate {
   public:
    explicit TabHelperDelegate(Browser* browser);
    ~TabHelperDelegate() override;

    base::WeakPtr<TabHelperDelegate> AsWeakPtr();

    // Called whenever scene activation level changed.
    void SceneActivationLevelChanged();

   private:
    // AppLauncherTabHelperDelegate:
    void LaunchAppForTabHelper(
        AppLauncherTabHelper* tab_helper,
        const GURL& url,
        base::OnceCallback<void(bool)> completion,
        base::OnceCallback<void()> back_completion) override;
    void ShowAppLaunchAlert(AppLauncherTabHelper* tab_helper,
                            AppLauncherAlertCause cause,
                            base::OnceCallback<void(bool)> completion) override;

    // Returns the OverlayRequestQueue to use for app launch dialogs from
    // `web_state`.  Returns the queue for `web_state`'s opener if `web_state`
    // is expected to be closed before the app launcher dialog can be shown.
    OverlayRequestQueue* GetQueueForAppLaunchDialog(web::WebState* web_state);

    // The Browser.  Used to fetch the appropriate request queue for app
    // launcher dialogs.
    raw_ptr<Browser> browser_ = nullptr;

    // Callback called in `UIApplication openURL:...` completion.
    // Parameter is the success parameter returned by the openURL API.
    base::OnceCallback<void(bool)> app_launch_completion_;

    // If app_launch_completion_ was called with `true`, this callback will be
    // called on next time scene leave the foreground/inactive state.
    base::OnceCallback<void(void)> back_to_app_completion_;

    // Private method called on `UIApplication openURL:...` completion.
    void OnAppLaunchCompleted(bool success);

    // Must be last member to ensure it is destroyed last.
    base::WeakPtrFactory<TabHelperDelegate> weak_factory_{this};
  };

  // Handler for app launches in the Browser.
  TabHelperDelegate tab_helper_delegate_;
  // The tab helper delegate installer.
  TabHelperDelegateInstaller<AppLauncherTabHelper, AppLauncherTabHelperDelegate>
      tab_helper_delegate_installer_;

  // A helper class to observer SceneState activation state.
  AppLauncherSceneStateObserver* app_launcher_scene_state_observer_;

  // BrowserUserData key.
  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_BROWSER_AGENT_H_
