// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONSENT_PROVIDER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONSENT_PROVIDER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Chrome-side enum identifying a boolean setting managed by the Gemini Live SDK.
typedef NS_ENUM(NSInteger, GeminiSetting) {
  // Whether live captions are enabled.
  GeminiSettingLiveCaptions = 0,
};

// Delegate for Gemini consent provider.
@protocol GeminiConsentProviderDelegate <NSObject>

// Returns YES if user has accepted Gemini Live consent.
- (BOOL)isGeminiLiveConsentAccepted;

// Returns YES if user has seen Gemini Live intro.
- (BOOL)isGeminiLiveIntroShown;

// Returns YES if microphone access has been granted.
- (BOOL)hasMicrophoneAccess;

@optional
// Returns the current value of a boolean setting.
- (BOOL)readSetting:(GeminiSetting)setting;

// Updates a boolean setting.
- (void)updateSetting:(GeminiSetting)setting enabled:(BOOL)enabled;

// A callback block fired whenever the host application independently changes
// any setting.
@property(nonatomic, copy, nullable) void (^settingUpdatedCallback)
    (BOOL enabled, GeminiSetting setting);

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONSENT_PROVIDER_DELEGATE_H_
