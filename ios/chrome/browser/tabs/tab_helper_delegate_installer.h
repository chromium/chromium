// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_HELPER_DELEGATE_INSTALLER_H_
#define IOS_CHROME_BROWSER_TABS_TAB_HELPER_DELEGATE_INSTALLER_H_

#include "base/check.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

// TabHelperDelegateInstaller is used to install a single delegate instance as
// the delegate for the tab helper associated with every WebState in a Browser's
// WebStateList.  When a TabHelperDelegateInstaller or its associated Browser is
// destroyed, all tab helpers' delegates are reset to nullptr.  Useful when a
// Browser-scoped object acts as the delegate for every tab helper.
//
// Usage example for a BrowserUserData that owns the tab helper delegates for
// FooTabHelper.  Two installers are used, one for the main delegate that is set
// via FooTabHelper::SetDelegate(), and one for a UI delegate that is set via
// FooTabHelper::SetUIDelegate():
//
//   class FooBrowserAgent : public BrowserUserData<FooBrowserAgent> {
//    public:
//     ...
//    private:
//     explicit FooBrowserAgent(Browser* browser)
//       : delegate_installer_(&tab_helper_delegate_, browser),
//         ui_delegate_installer_(&tab_helper_ui_delegate_, browser) {}
//
//     FooTabHelperDelegate tab_helper_delegate_;
//     TabHelperDelegateInstaller<FooTabHelper,
//                                FooTabHelperDelegate> delegate_installer_;
//
//     FooTabHelperUIDelegate tab_helper_ui_delegate_;
//     TabHelperDelegateInstaller<FooTabHelper,
//                                FooTabHelperUIDelegate,
//                                &FooTabHelper::SetUIDelegate>
//       ui_delegate_installer_;
//   };
template <class Helper,
          class Delegate,
          void (Helper::*SetDelFn)(Delegate*) = &Helper::SetDelegate>
class TabHelperDelegateInstaller {
 public:
  TabHelperDelegateInstaller(Delegate* delegate, Browser* browser)
      : installer_(delegate, browser->GetWebStateList()),
        shutdown_helper_(&installer_, browser) {}
  ~TabHelperDelegateInstaller() = default;

  TabHelperDelegateInstaller(const TabHelperDelegateInstaller&) = delete;
  TabHelperDelegateInstaller& operator=(const TabHelperDelegateInstaller&) =
      delete;

 private:
  // Helper object that sets up the delegates for all Helpers in a Browser's
  // WebStateList.
  class Installer : public WebStateListObserver {
   public:
    Installer(Delegate* delegate, WebStateList* web_state_list)
        : delegate_(delegate), web_state_list_(web_state_list) {
      DCHECK(delegate_);
      DCHECK(web_state_list_);
      scoped_observation_.Observe(web_state_list_);
      for (int i = 0; i < web_state_list_->count(); ++i) {
        SetTabHelperDelegate(web_state_list_->GetWebStateAt(i), delegate_);
      }
    }
    ~Installer() override { Disconnect(); }

    // Uninstalls the delegate and stops observing the WebStateList.
    void Disconnect() {
      if (!web_state_list_)
        return;
      for (int i = 0; i < web_state_list_->count(); ++i) {
        SetTabHelperDelegate(web_state_list_->GetWebStateAt(i), nullptr);
      }
      DCHECK(scoped_observation_.IsObservingSource(web_state_list_));
      scoped_observation_.Reset();
      web_state_list_ = nullptr;
    }

   private:
    // WebStateListObserver:
    void WebStateInsertedAt(WebStateList* web_state_list,
                            web::WebState* web_state,
                            int index,
                            bool activating) override {
      SetTabHelperDelegate(web_state, delegate_);
    }
    void WebStateReplacedAt(WebStateList* web_state_list,
                            web::WebState* old_web_state,
                            web::WebState* new_web_state,
                            int index) override {
      SetTabHelperDelegate(old_web_state, nullptr);
      SetTabHelperDelegate(new_web_state, delegate_);
    }
    void WillDetachWebStateAt(WebStateList* web_state_list,
                              web::WebState* web_state,
                              int index) override {
      SetTabHelperDelegate(web_state, nullptr);
    }

    // Sets the delegate for `web_state`'s Helper to `delegate`.
    void SetTabHelperDelegate(web::WebState* web_state, Delegate* delegate) {
      (Helper::FromWebState(web_state)->*SetDelFn)(delegate);
    }

    // The delegate that is installed for each WebState in the WebStateList.
    Delegate* delegate_ = nullptr;
    // The WebStateList whose Helpers's delegates are being installed.
    WebStateList* web_state_list_ = nullptr;
    // Scoped observer for `web_state_list_`.
    base::ScopedObservation<WebStateList, WebStateListObserver>
        scoped_observation_{this};
  };

  // Helper object that sets up the delegate installer and tears it down
  // when the Browser is destroyed.
  class BrowserShutdownHelper : public BrowserObserver {
   public:
    BrowserShutdownHelper(Installer* installer, Browser* browser)
        : installer_(installer) {
      DCHECK(installer_);
      DCHECK(browser);
      scoped_observation_.Observe(browser);
    }
    ~BrowserShutdownHelper() override = default;

   private:
    // BrowserObserver:
    void BrowserDestroyed(Browser* browser) override {
      installer_->Disconnect();
      DCHECK(scoped_observation_.IsObservingSource(browser));
      scoped_observation_.Reset();
    }

    // The installer used to set up the Delegates.
    Installer* installer_ = nullptr;
    // Scoped observer for the Browser.
    base::ScopedObservation<Browser, BrowserObserver> scoped_observation_{this};
  };

  // Helper object that installs delegates for all the Browser's tab helpers.
  Installer installer_;
  // Helper object that sets up and tears down the Installer for a Browser.
  BrowserShutdownHelper shutdown_helper_;
};

#endif  // IOS_CHROME_BROWSER_TABS_TAB_HELPER_DELEGATE_INSTALLER_H_
