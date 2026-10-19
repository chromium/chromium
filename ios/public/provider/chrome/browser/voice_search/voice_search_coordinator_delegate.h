// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_COORDINATOR_DELEGATE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class ChromeCoordinator;

// Delegate for the coordinator presenting the Voice Plate, which reports the
// recognized query and the Voice Plate's dismissal.
@protocol VoiceSearchCoordinatorDelegate <NSObject>

// Called when `coordinator` recognized `query`. The delegate may stop
// `coordinator` in response.
- (void)voiceSearchCoordinator:(ChromeCoordinator*)coordinator
             didRecognizeQuery:(NSString*)query;

// Called when the Voice Plate was dismissed. The owner is expected to stop
// `coordinator`.
- (void)voiceSearchCoordinatorDidFinish:(ChromeCoordinator*)coordinator;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_COORDINATOR_DELEGATE_H_
