// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/active_contextual_panel_tab_helper_observation_forwarder.h"

#import "base/check.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"

ActiveContextualPanelTabHelperObservationForwarder::
    ActiveContextualPanelTabHelperObservationForwarder(
        WebStateList* web_state_list,
        ContextualPanelTabHelperObserver* observer)
    : tab_helper_observation_(observer) {
  DCHECK(observer);
  DCHECK(web_state_list);
  web_state_list_observation_.Observe(web_state_list);

  web::WebState* active_web_state = web_state_list->GetActiveWebState();
  if (active_web_state) {
    ContextualPanelTabHelper* tab_helper =
        ContextualPanelTabHelper::FromWebState(active_web_state);
    if (tab_helper) {
      tab_helper_observation_.Observe(tab_helper);
    }
  }
}

ActiveContextualPanelTabHelperObservationForwarder::
    ~ActiveContextualPanelTabHelperObservationForwarder() {}

#pragma mark - WebStateListObserver

void ActiveContextualPanelTabHelperObservationForwarder::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (!status.active_web_state_change()) {
    return;
  }

  tab_helper_observation_.Reset();
  if (status.new_active_web_state) {
    ContextualPanelTabHelper* tab_helper =
        ContextualPanelTabHelper::FromWebState(status.new_active_web_state);
    if (tab_helper) {
      tab_helper_observation_.Observe(tab_helper);
    }
  }
}
