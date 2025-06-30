// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/scoped_observation.h"
#import "base/types/expected.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

enum class PageContextWrapperError;

@protocol BWGCommands;

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

// Tab helper controlling the BWG feature and its current state for a given tab.
class BwgTabHelper : public web::WebStateObserver,
                     public web::WebStateUserData<BwgTabHelper> {
 public:
  BwgTabHelper(const BwgTabHelper&) = delete;
  BwgTabHelper& operator=(const BwgTabHelper&) = delete;

  ~BwgTabHelper() override;

  // Presents the BWG overlay on the given view controller with the correct
  // client and server IDs for the associated WebState.
  void PresentBwgOverlay(
      UIViewController* base_view_controller,
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> expected_page_context);

  // Sets the state of `is_bwg_session_active_`.
  void SetBwgSessionActive(bool active);

  // Set the BWG commands handler, used to show/hide the BWG UI.
  void SetBwgCommandsHandler(id<BWGCommands> handler);

  // WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  explicit BwgTabHelper(web::WebState* web_state);

  friend class web::WebStateUserData<BwgTabHelper>;

  // Gets the client and server IDs for the BWG session for the associated
  // WebState. server ID is optional because it may not be found or valid.
  std::string GetClientId();
  std::optional<std::string> GetServerId();

  // WebState this tab helper is attached to.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Whether the BWG session is currently active.
  bool is_bwg_session_active_ = false;

  // Commands handler for BWG commands.
  __weak id<BWGCommands> bwg_commands_handler_ = nil;

  // The observation of the Web State.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_
