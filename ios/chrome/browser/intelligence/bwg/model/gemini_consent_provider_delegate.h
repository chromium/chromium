// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONSENT_PROVIDER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONSENT_PROVIDER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for Gemini consent provider.
@protocol GeminiConsentProviderDelegate <NSObject>

// Returns YES if user has accepted Gemini Live consent.
- (BOOL)isGeminiLiveConsentAccepted;

// Returns YES if user has seen Gemini Live intro.
- (BOOL)isGeminiLiveIntroShown;

// Returns YES if microphone access has been granted.
- (BOOL)hasMicrophoneAccess;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONSENT_PROVIDER_DELEGATE_H_
