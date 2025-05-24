// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class TextToSpeechPlaybackController;

// TextToSpeechPlaybackControllerFactory attaches
// TextToSpeechPlaybackControllers to ProfileIOSs.
class TextToSpeechPlaybackControllerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static TextToSpeechPlaybackController* GetForProfile(ProfileIOS* profile);
  // Getter for singleton instance.
  static TextToSpeechPlaybackControllerFactory* GetInstance();

 private:
  friend class base::NoDestructor<TextToSpeechPlaybackControllerFactory>;

  TextToSpeechPlaybackControllerFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_
