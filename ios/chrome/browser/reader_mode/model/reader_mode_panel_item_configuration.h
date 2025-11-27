// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_PANEL_ITEM_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_PANEL_ITEM_CONFIGURATION_H_

#import "base/scoped_observation.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/web/public/web_state_observer.h"

// Configuration for the Reader Mode item in the contextual panel.
// This object observes the WebState and the ReaderModeTabHelper to manage its
// own lifecycle.
class ReaderModePanelItemConfiguration
    : public ContextualPanelItemConfiguration,
      public web::WebStateObserver,
      public ReaderModeTabHelper::Observer {
 public:
  explicit ReaderModePanelItemConfiguration(web::WebState* web_state);
  ~ReaderModePanelItemConfiguration() override;

  // ContextualPanelItemConfiguration
  void DidTransitionToSmallEntrypoint() override;

  // ReaderModeTabHelper::Observer
  void ReaderModeTabHelperDestroyed(ReaderModeTabHelper* tab_helper,
                                    web::WebState* web_state) override;
  void ReaderModeWebStateDidLoadContent(ReaderModeTabHelper* tab_helper,
                                        web::WebState* web_state) override;
  void ReaderModeWebStateWillBecomeUnavailable(
      ReaderModeTabHelper* tab_helper,
      web::WebState* web_state,
      ReaderModeDeactivationReason reason) override;
  void ReaderModeDistillationFailed(ReaderModeTabHelper* tab_helper) override;

  // web::WebStateObserver
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // Invalidates this configuration.
  void Invalidate();

  // Helper which returns whether the profile is eligible for BWG.
  bool IsProfileEligibleForBwg();

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  base::ScopedObservation<ReaderModeTabHelper, ReaderModeTabHelper::Observer>
      reader_mode_tab_helper_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_PANEL_ITEM_CONFIGURATION_H_
