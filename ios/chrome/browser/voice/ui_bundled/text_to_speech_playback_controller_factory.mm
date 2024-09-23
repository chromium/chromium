// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller.h"

// static
TextToSpeechPlaybackController*
TextToSpeechPlaybackControllerFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<TextToSpeechPlaybackController*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
TextToSpeechPlaybackControllerFactory*
TextToSpeechPlaybackControllerFactory::GetInstance() {
  static base::NoDestructor<TextToSpeechPlaybackControllerFactory> instance;
  return instance.get();
}

TextToSpeechPlaybackControllerFactory::TextToSpeechPlaybackControllerFactory()
    : BrowserStateKeyedServiceFactory(
          "TextToSpeechPlaybackController",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
TextToSpeechPlaybackControllerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<TextToSpeechPlaybackController>();
}

web::BrowserState* TextToSpeechPlaybackControllerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
