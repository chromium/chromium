// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol SceneCommands;

// Tab helper that listens for new tabs to triggers or not the cobrowse view.
class CobrowseTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<CobrowseTabHelper> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual bool CanShowAssistantForWebState(web::WebState* web_state) = 0;
  };

  CobrowseTabHelper(const CobrowseTabHelper&) = delete;
  CobrowseTabHelper& operator=(const CobrowseTabHelper&) = delete;

  ~CobrowseTabHelper() override;

  // Sets the scene commands handler.
  void SetSceneCommandsHandler(id<SceneCommands> handler);

  // Sets the delegate.
  void SetDelegate(Delegate* delegate);

  // WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<CobrowseTabHelper>;

  explicit CobrowseTabHelper(web::WebState* web_state);

  // The delegate for this tab helper.
  raw_ptr<Delegate> delegate_ = nullptr;

  // The handler for scene commands.
  __weak id<SceneCommands> scene_commands_handler_ = nil;

  // Scoped observation for the WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
};

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_COBROWSE_TAB_HELPER_H_
