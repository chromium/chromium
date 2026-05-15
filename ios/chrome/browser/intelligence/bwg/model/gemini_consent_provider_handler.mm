// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_consent_provider_handler.h"

#import <AVFoundation/AVFoundation.h>

#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"

@implementation GeminiConsentProviderHandler {
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - GeminiConsentProviderDelegate

- (BOOL)isGeminiLiveConsentAccepted {
  return gemini::DidUserConsentToGemini(_prefService);
}

- (BOOL)isGeminiLiveIntroShown {
  return gemini::DidGeminiLiveIntroPlay(_prefService);
}

- (BOOL)hasMicrophoneAccess {
  return [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio] ==
         AVAuthorizationStatusAuthorized;
}

@end
