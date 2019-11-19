// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

@class TextToSpeechNotificationHandler;
class WebStateList;

// A helper object that listens for TTS notifications and manages playback.
class TextToSpeechPlaybackController : public KeyedService,
                                       public WebStateListObserver,
                                       public web::WebStateObserver {
 public:
  explicit TextToSpeechPlaybackController();

  // The BrowserState's WebStateList.
  void SetWebStateList(WebStateList* web_state_list);

  // Whether TTS playback is enabled.
  bool IsEnabled() const;
  void SetEnabled(bool enabled);

 private:
  // Setter for the current WebState being observed.
  void SetWebState(web::WebState* web_state);

  // KeyedService:
  void Shutdown() override;

  // WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           int reason) override;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;
  void WillCloseWebStateAt(WebStateList* web_state_list,
                           web::WebState* web_state,
                           int index,
                           bool user_action) override;

  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Helper object that listens for TTS notifications.
  __strong TextToSpeechNotificationHandler* notification_helper_ = nil;

  // The WebStateList.
  WebStateList* web_state_list_ = nullptr;

  // The WebState being observed.
  web::WebState* web_state_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TextToSpeechPlaybackController);
};

#endif  // IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_H_
