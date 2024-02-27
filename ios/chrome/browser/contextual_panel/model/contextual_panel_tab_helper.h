// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#include "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class ContextualPanelModel;
struct ContextualPanelItemConfiguration;
class ContextualPanelTabHelperObserver;

// Tab helper controlling the Contextual Panel feature for a given tab.
class ContextualPanelTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ContextualPanelTabHelper> {
 public:
  ContextualPanelTabHelper(const ContextualPanelTabHelper&) = delete;
  ContextualPanelTabHelper& operator=(const ContextualPanelTabHelper&) = delete;

  ~ContextualPanelTabHelper() override;

  // Adds and removes observers for contextual panel actions. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  void AddObserver(ContextualPanelTabHelperObserver* observer);
  void RemoveObserver(ContextualPanelTabHelperObserver* observer);

  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

 private:
  friend class web::WebStateUserData<ContextualPanelTabHelper>;

  ContextualPanelTabHelper(web::WebState* web_state);

  // Callback for when the given model has finished fetching its data.
  void ModelCallbackReceived(base::WeakPtr<ContextualPanelModel> model,
                             ContextualPanelItemConfiguration configuration);

  // Removes any deallocated models from the model list.
  void CleanUpModels();

  WEB_STATE_USER_DATA_KEY_DECL();

  // List of the models this tab helper should query for possible panels.
  std::vector<base::WeakPtr<ContextualPanelModel>> models_;

  // List of observers to be notified when the Contextual Panel gets new data.
  base::ObserverList<ContextualPanelTabHelperObserver, true> observers_;

  // Scoped observation for WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  base::WeakPtrFactory<ContextualPanelTabHelper> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_
