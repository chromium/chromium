// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_ACTIVE_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVATION_FORWARDER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_ACTIVE_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVATION_FORWARDER_H_

#include "base/scoped_observation.h"
#include "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class ContextualPanelTabHelper;

// ActiveContextualPanelTabHelperObservationForwarder forwards
// ContextualPanelTabHelperObserver methods for the ContextualPanelTabHelper for
// the active WebState in a WebStateList, handling cases where the active
// WebState changes.
class ActiveContextualPanelTabHelperObservationForwarder
    : public WebStateListObserver {
 public:
  // Creates an object which forwards observation methods to `observer` and
  // tracks `web_state_list` to keep track of the currently-active WebState.
  // `web_state_list` and `observer` must both outlive this object.
  ActiveContextualPanelTabHelperObservationForwarder(
      WebStateList* web_state_list,
      ContextualPanelTabHelperObserver* observer);

  ActiveContextualPanelTabHelperObservationForwarder(
      const ActiveContextualPanelTabHelperObservationForwarder&) = delete;
  ActiveContextualPanelTabHelperObservationForwarder& operator=(
      const ActiveContextualPanelTabHelperObservationForwarder&) = delete;

  ~ActiveContextualPanelTabHelperObservationForwarder() override;

  // WebStateListObserver.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

 private:
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  base::ScopedObservation<ContextualPanelTabHelper,
                          ContextualPanelTabHelperObserver>
      tab_helper_observation_;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_ACTIVE_CONTEXTUAL_PANEL_TAB_HELPER_OBSERVATION_FORWARDER_H_
