// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_H_

#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent_delegate.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_dependency_bridge.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state_observer.h"

@protocol ReaderModeCommands;
@protocol ReaderModeChipCommands;
class ReaderModeTabHelper;
class WebStateList;

// Observes the WebStateList of the associated browser and ensures the Reader
// mode UI is presented and dismissed accordingly when there is a new active
// WebState or when Reader mode content becomes available/unavailable in the
// currently active WebState.
class ReaderModeBrowserAgent : public BrowserUserData<ReaderModeBrowserAgent>,
                               public WebStateListObserver,
                               public web::WebStateObserver,
                               public ReaderModeTabHelper::Observer {
 public:
  ReaderModeBrowserAgent(const ReaderModeBrowserAgent&) = delete;
  ReaderModeBrowserAgent& operator=(const ReaderModeBrowserAgent&) = delete;

  ~ReaderModeBrowserAgent() override;

  // Sets the `delegate_`.
  void SetDelegate(id<ReaderModeBrowserAgentDelegate> delegate);

 private:
  friend class BrowserUserData<ReaderModeBrowserAgent>;

  explicit ReaderModeBrowserAgent(Browser* browser);

  // Helper returning the active WebState in the browser.
  web::WebState* GetActiveWebState();
  // Attaches a WebState to this browser agent.
  void AttachWebState(web::WebState* web_state);
  // Detaches a WebState from this browser agent.
  void DetachWebState(web::WebState* web_state);
  // Returns the ReaderModeTabHelper of the active WebState.
  ReaderModeTabHelper* GetActiveReaderModeTabHelper();
  // Attaches a ReaderModeTabHelper to this browser agent.
  void AttachReaderModeTabHelper(ReaderModeTabHelper* tab_helper);
  // Detaches a ReaderModeTabHelper from this browser agent.
  void DetachReaderModeTabHelper(ReaderModeTabHelper* tab_helper);

  // Show/hide the Reader mode UI.
  void ShowReaderModeUI(BOOL animated);
  void HideReaderModeUI(BOOL animated);

  // Updates any handlers that rely on the non-Reading mode web state when the
  // Reading mode web state has changed.
  void UpdateHandlersOnActiveWebState();

  // WebStateListObserver methods.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

  // web::WebStateObserver methods.
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // ReaderModeTabHelper::Observer methods.
  void ReaderModeWebStateDidLoadContent(ReaderModeTabHelper* tab_helper,
                                        web::WebState* web_state) override;
  void ReaderModeWebStateWillBecomeUnavailable(
      ReaderModeTabHelper* tab_helper,
      web::WebState* web_state,
      ReaderModeDeactivationReason reason) override;
  void ReaderModeDistillationFailed(ReaderModeTabHelper* tab_helper) override;
  void ReaderModeTabHelperDestroyed(ReaderModeTabHelper* tab_helper,
                                    web::WebState* web_state) override;

  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_scoped_observation_{this};
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_scoped_observation_{this};
  base::ScopedMultiSourceObservation<ReaderModeTabHelper,
                                     ReaderModeTabHelper::Observer>
      reader_mode_tab_helper_scoped_observation_{this};

  // The delegate for this agent.
  id<ReaderModeBrowserAgentDelegate> delegate_;
  ReaderModeDependencyBridge bridge_;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_H_
