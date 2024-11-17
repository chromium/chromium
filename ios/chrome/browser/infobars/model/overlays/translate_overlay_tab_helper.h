// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_TRANSLATE_OVERLAY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_TRANSLATE_OVERLAY_TAB_HELPER_H_

#include <string>

#import "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}
class InfoBarIOS;
class OverlayRequestQueue;
class InfobarOverlayRequestInserter;
class TipsManagerIOS;

// Helper object that inserts a Translate banner request when Translate finishes
// for an infobar.
class TranslateOverlayTabHelper
    : public web::WebStateUserData<TranslateOverlayTabHelper> {
 public:
  ~TranslateOverlayTabHelper() override;

  // Observer to listen for Translate completions.
  class Observer : public base::CheckedObserver {
   public:
    virtual void TranslationFinished(TranslateOverlayTabHelper* tab_helper,
                                     bool success) {}
    virtual void TranslateOverlayTabHelperDestroyed(
        TranslateOverlayTabHelper* tab_helper) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  TranslateOverlayTabHelper(web::WebState* web_state);

  // Observers to listen to translation completions.
  base::ObserverList<Observer, true> observers_;

 private:
  friend class web::WebStateUserData<TranslateOverlayTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Observes a Translate InfoBar for changes to the TranslateStep.
  class TranslateStepObserver
      : public translate::TranslateInfoBarDelegate::Observer {
   public:
    TranslateStepObserver(TranslateOverlayTabHelper* tab_helper);
    ~TranslateStepObserver() override;

    // Starts observing `infobar`'s delegate, stores `infobar` for
    // TranslateDid[Start/Finish]
    void SetTranslateInfoBar(InfoBarIOS* infobar);

   private:
    // translate::TranslateInfoBarDelegate::Observer.
    void OnTranslateStepChanged(translate::TranslateStep step,
                                translate::TranslateErrors error_type) override;
    void OnTargetLanguageChanged(
        const std::string& target_language_code) override;
    bool IsDeclinedByUser() override;
    void OnTranslateInfoBarDelegateDestroyed(
        translate::TranslateInfoBarDelegate* delegate) override;

    // Scoped observer that facilitates observing a TranslateInfoBarDelegate.
    base::ScopedObservation<translate::TranslateInfoBarDelegate,
                            translate::TranslateInfoBarDelegate::Observer>
        translate_scoped_observation_{this};
    // TranslateOverlayTabHelper instance.
    raw_ptr<TranslateOverlayTabHelper> tab_helper_;
    raw_ptr<infobars::InfoBar> translate_infobar_ = nil;
  };

  // Observes a WebState's InfoBarManager for Translate Infobars.
  class TranslateInfobarObserver : public infobars::InfoBarManager::Observer {
   public:
    TranslateInfobarObserver(web::WebState* web_state,
                             TranslateOverlayTabHelper* tab_helper);
    ~TranslateInfobarObserver() override;

   private:
    // infobars::InfoBarManager::Observer:
    void OnInfoBarAdded(infobars::InfoBar* infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

    // Scoped observer that facilitates observing an InfoBarManager
    base::ScopedObservation<infobars::InfoBarManager,
                            infobars::InfoBarManager::Observer>
        infobar_manager_scoped_observation_{this};
    // TranslateOverlayTabHelper instance.
    raw_ptr<TranslateOverlayTabHelper> tab_helper_;
    // Weak pointer to the `TipsManagerIOS`.
    raw_ptr<TipsManagerIOS> tips_manager_;
  };

  // Listens for a WebStateDestroyed callback to null out any WebState-scoped
  // pointers.
  class WebStateDestroyedObserver : public web::WebStateObserver {
   public:
    WebStateDestroyedObserver(web::WebState* web_state,
                              TranslateOverlayTabHelper* tab_helper);
    ~WebStateDestroyedObserver() override;

   private:
    // WebStateObserver
    void WebStateDestroyed(web::WebState* web_state) override;

    // Scoped observer that facilitates observing an InfoBarManager
    base::ScopedObservation<web::WebState, web::WebStateObserver>
        web_state_scoped_observation_{this};
    // TranslateOverlayTabHelper instance.
    raw_ptr<TranslateOverlayTabHelper> tab_helper_;
  };

  // Inserts the placeholder after the initial banner if one is in the queue.
  void TranslateDidStart(infobars::InfoBar* infobar);
  // If successful, inserts the secondary banner request after the placeholder,
  // or triggers the snackbar if unsuccessful.  Then send
  // Observer::TranslationFinished() to cancel the placeholder if one exists.
  void TranslateDidFinish(infobars::InfoBar* infobar, bool success);
  // Indicates the addition of a Translate infobar.
  void TranslateInfoBarAdded(InfoBarIOS* infobar);
  // Indicates that this TabHelper's WebState has been destroyed.
  void UpdateForWebStateDestroyed();

  TranslateStepObserver translate_step_observer_;
  TranslateInfobarObserver translate_infobar_observer_;
  WebStateDestroyedObserver web_state_observer_;

  // Banner queue for the TabHelper's WebState;
  raw_ptr<OverlayRequestQueue> banner_queue_ = nullptr;
  // Request inserter for the TabHelper's WebState;
  raw_ptr<InfobarOverlayRequestInserter> inserter_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_TRANSLATE_OVERLAY_TAB_HELPER_H_
