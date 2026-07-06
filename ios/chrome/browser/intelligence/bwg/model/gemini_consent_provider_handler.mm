// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_consent_provider_handler.h"

#import <AVFoundation/AVFoundation.h>

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@interface GeminiConsentProviderHandler () <PrefObserverDelegate>
@end

@implementation GeminiConsentProviderHandler {
  raw_ptr<PrefService> _prefService;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

@synthesize settingUpdatedCallback = _settingUpdatedCallback;

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(_prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIOSGeminiLiveClosedCaptioningSetting,
        _prefChangeRegistrar.get());
  }
  return self;
}

- (void)disconnect {
  _prefObserverBridge.reset();
  _prefChangeRegistrar.reset();
  _prefService = nullptr;
}

#pragma mark - GeminiConsentProviderDelegate

- (BOOL)isGeminiLiveConsentAccepted {
  return gemini::DidUserConsentToGeminiLive(_prefService);
}

- (BOOL)isGeminiLiveIntroShown {
  return gemini::DidGeminiLiveIntroPlay(_prefService);
}

- (BOOL)hasMicrophoneAccess {
  if (!_prefService->GetBoolean(prefs::kIOSGeminiLiveMicrophoneSetting)) {
    return NO;
  }
  return [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio] ==
         AVAuthorizationStatusAuthorized;
}

- (BOOL)readSetting:(GeminiSetting)setting {
  switch (setting) {
    case GeminiSettingLiveCaptions:
      return _prefService->GetBoolean(prefs::kIOSGeminiLiveClosedCaptioningSetting);
  }
}

- (void)updateSetting:(GeminiSetting)setting enabled:(BOOL)enabled {
  switch (setting) {
    case GeminiSettingLiveCaptions:
      _prefService->SetBoolean(prefs::kIOSGeminiLiveClosedCaptioningSetting,
                               enabled);
      break;
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIOSGeminiLiveClosedCaptioningSetting) {
    BOOL closedCaptioningEnabled =
        _prefService->GetBoolean(prefs::kIOSGeminiLiveClosedCaptioningSetting);
    if (self.settingUpdatedCallback) {
      self.settingUpdatedCallback(closedCaptioningEnabled,
                                  GeminiSettingLiveCaptions);
    }
  }
}

@end
