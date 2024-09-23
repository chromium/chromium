// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_H_
#define IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

@class TextToSpeechNotificationHandler;
class WebStateList;

// A helper object that listens for TTS notifications and manages playback.
class TextToSpeechPlaybackController : public KeyedService,
                                       public WebStateListObserver,
                                       public web::WebStateObserver {
 public:
  explicit TextToSpeechPlaybackController();

  TextToSpeechPlaybackController(const TextToSpeechPlaybackController&) =
      delete;
  TextToSpeechPlaybackController& operator=(
      const TextToSpeechPlaybackController&) = delete;

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
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Helper object that listens for TTS notifications.
  __strong TextToSpeechNotificationHandler* notification_helper_ = nil;

  // The WebStateList.
  raw_ptr<WebStateList> web_state_list_ = nullptr;

  // The WebState being observed.
  raw_ptr<web::WebState> web_state_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_H_
