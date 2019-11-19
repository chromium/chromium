// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SAD_TAB_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_SAD_TAB_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol SadTabTabHelperDelegate;
class ScopedFullscreenDisabler;

// SadTabTabHelper listens to RenderProcessGone events and presents a
// SadTabView view appropriately.
class SadTabTabHelper : public web::WebStateUserData<SadTabTabHelper>,
                        public web::WebStateObserver {
 public:
  // Creates a SadTabTabHelper and attaches it to a specific web_state object.
  // Uses a default repeat_failure_interval.
  static void CreateForWebState(web::WebState* web_state);

  // Creates a SadTabTabHelper and attaches it to a specific web_state object,
  // |repeat_failure_interval| sets the corresponding instance variable used for
  // determining repeat failures.
  static void CreateForWebState(web::WebState* web_state,
                                double repeat_failure_interval);

  ~SadTabTabHelper() override;

  // Sets the SadTabHelper delegate. |delegate| will be in charge of presenting
  // the SadTabView. |delegate| is not retained by TabHelper.
  void SetDelegate(id<SadTabTabHelperDelegate> delegate);

  // true if Sad Tab has currently being shown.
  bool is_showing_sad_tab() const { return showing_sad_tab_; }

 private:
  friend class web::WebStateUserData<SadTabTabHelper>;

  // Constructs a SadTabTabHelper, assigning the helper to a web_state. A
  // default repeat_failure_interval will be used.
  explicit SadTabTabHelper(web::WebState* web_state);

  // Constructs a SadTabTabHelper allowing an optional |repeat_failure_interval|
  // value to be passed in, representing a timeout period in seconds during
  // which a second failure will be considered a 'repeated' crash rather than an
  // initial event.
  SadTabTabHelper(web::WebState* web_state, double repeat_failure_interval);

  // Presents a new SadTabView via the web_state object.
  void PresentSadTab(const GURL& url_causing_failure);

  // Called when the Sad Tab is added or removed from the WebState's content
  // area.
  void SetIsShowingSadTab(bool showing_sad_tab);

  // Loads the current page after renderer crash while displaying the page
  // placeholder during the load. Reloading the page which was not visible to
  // the user during the crash is a better user experience than presenting
  // Sad Tab.
  void ReloadTab();

  // Called when the app becomes active.
  void OnAppDidBecomeActive();

  // Adds UIApplicationDidBecomeActiveNotification observer.
  void AddApplicationDidBecomeActiveObserver();
  // Removes UIApplicationDidBecomeActiveNotification observer.
  void RemoveApplicationDidBecomeActiveObserver();

  // Creates or resets the fullscreen disabler depending on whether the sad tab
  // is currently visible.
  void UpdateFullscreenDisabler();

  // WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The default window of time a failure of the same URL needs to occur
  // to be considered a repeat failure.
  static const double kDefaultRepeatFailureInterval;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Stores the last URL that caused a renderer crash,
  // used to detect repeated crashes.
  GURL last_failed_url_;

  // Stores the last date that the renderer crashed,
  // used to determine time window for repeated crashes.
  std::unique_ptr<base::ElapsedTimer> last_failed_timer_;

  // Whether a Sad Tab is being shown over |web_state_|'s content area.
  bool showing_sad_tab_ = false;

  // true if Sad Tab is presented and presented for repeated load failure.
  bool repeated_failure_ = false;

  // The fullscreen disabler for when the sad tab is visible.
  std::unique_ptr<ScopedFullscreenDisabler> fullscreen_disabler_;

  // Stores the interval window in seconds during which a second
  // RenderProcessGone failure will be considered a repeat failure.
  double repeat_failure_interval_ = kDefaultRepeatFailureInterval;

  // true if the WebState needs to be reloaded after web state becomes visible.
  bool requires_reload_on_becoming_visible_ = false;

  // true if the WebState needs to be reloaded after the app becomes active.
  bool requires_reload_on_becoming_active_ = false;

  // Delegate which displays the SadTabView.
  __weak id<SadTabTabHelperDelegate> delegate_ = nil;

  // Observer for UIApplicationDidBecomeActiveNotification.
  __strong id<NSObject> application_did_become_active_observer_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SadTabTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_SAD_TAB_TAB_HELPER_H_
