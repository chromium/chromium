// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller_factory.h"

#import <memory>
#import <utility>

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/voice/ui_bundled/text_to_speech_playback_controller.h"

TextToSpeechPlaybackControllerFactory::TextToSpeechPlaybackControllerFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "TextToSpeechPlaybackController",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService>
TextToSpeechPlaybackControllerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<TextToSpeechPlaybackController>();
}
