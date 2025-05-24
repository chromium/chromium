// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller.h"

// static
TextToSpeechPlaybackController*
TextToSpeechPlaybackControllerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<TextToSpeechPlaybackController>(
      profile, /*create=*/true);
}

// static
TextToSpeechPlaybackControllerFactory*
TextToSpeechPlaybackControllerFactory::GetInstance() {
  static base::NoDestructor<TextToSpeechPlaybackControllerFactory> instance;
  return instance.get();
}

TextToSpeechPlaybackControllerFactory::TextToSpeechPlaybackControllerFactory()
    : ProfileKeyedServiceFactoryIOS("TextToSpeechPlaybackController",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

std::unique_ptr<KeyedService>
TextToSpeechPlaybackControllerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<TextToSpeechPlaybackController>();
}
