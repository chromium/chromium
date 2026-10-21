// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller.h"

// TextToSpeechPlaybackControllerFactory attaches
// TextToSpeechPlaybackControllers to ProfileIOSs.
class TextToSpeechPlaybackControllerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          TextToSpeechPlaybackControllerFactory,
          TextToSpeechPlaybackController> {
 public:
  explicit TextToSpeechPlaybackControllerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_VOICE_UI_BUNDLED_TEXT_TO_SPEECH_PLAYBACK_CONTROLLER_FACTORY_H_
