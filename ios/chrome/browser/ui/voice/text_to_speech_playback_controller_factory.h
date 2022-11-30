// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class TextToSpeechPlaybackController;

// TextToSpeechPlaybackControllerFactory attaches
// TextToSpeechPlaybackControllers to ChromeBrowserStates.
class TextToSpeechPlaybackControllerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Convenience getter that typecasts the value returned to a
  // TextToSpeechPlaybackController.
  static TextToSpeechPlaybackController* GetForBrowserState(
      ChromeBrowserState* browser_state);
  // Getter for singleton instance.
  static TextToSpeechPlaybackControllerFactory* GetInstance();

  TextToSpeechPlaybackControllerFactory(
      const TextToSpeechPlaybackControllerFactory&) = delete;
  TextToSpeechPlaybackControllerFactory& operator=(
      const TextToSpeechPlaybackControllerFactory&) = delete;

 private:
  friend class base::NoDestructor<TextToSpeechPlaybackControllerFactory>;

  TextToSpeechPlaybackControllerFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_UI_VOICE_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_
