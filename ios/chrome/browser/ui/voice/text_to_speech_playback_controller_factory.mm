// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
TextToSpeechPlaybackController*
TextToSpeechPlaybackControllerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<TextToSpeechPlaybackController*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
