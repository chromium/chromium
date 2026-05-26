// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_GEMINI_LIVE_DELEGATE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_GEMINI_LIVE_DELEGATE_H_

#import <UIKit/UIKit.h>

// Protocol for receiving UI presentation requests and event signals from the
// Gemini Live session.
@protocol GeminiLiveDelegate <NSObject>

// Called when the SDK has shown the Live intro sequence. Chrome should
// update its preferences to record that the intro has been shown.
- (void)geminiLiveIntroShown:(UIViewController*)viewController;

// Called when the SDK detects that system microphone access is unavailable.
// Chrome must present a microphone permission alert and invoke the completion.
- (void)geminiLive:(UIViewController*)viewController
    showMicrophoneAlertWithCompletion:(void (^)(BOOL granted))completion;

// Called when the SDK needs Chrome to present the Live consent (FRE) screen.
// Chrome must present the FRE consent UI and invoke the completion.
- (void)geminiLive:(UIViewController*)viewController
    showConsentScreenWithCompletion:(void (^)(BOOL accepted))completion;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_GEMINI_LIVE_DELEGATE_H_
