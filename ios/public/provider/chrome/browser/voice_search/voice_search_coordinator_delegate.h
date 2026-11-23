// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_COORDINATOR_DELEGATE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class ChromeCoordinator;

// Delegate for the coordinator presenting the Voice Plate, which reports the
// recognized query, the user's request to edit it, and the end of the session.
// Callbacks aren't sent once the coordinator is stopped, except
// `-voiceSearchCoordinatorDidRequestRetry:`, and the owner may stop it from any
// callback.
@protocol VoiceSearchCoordinatorDelegate <NSObject>

// Called when `coordinator` recognized `query`, which is never empty. The owner
// is expected to stop `coordinator` and load `query`, marking the resulting
// navigation, if any, as a Voice Search result.
- (void)voiceSearchCoordinator:(ChromeCoordinator*)coordinator
             didRecognizeQuery:(NSString*)query;

// Called after the Voice Plate was dismissed for the user to edit `query`,
// instead of `-voiceSearchCoordinatorDidFinish:`. The owner is expected to stop
// `coordinator` and show `query`, which may be empty, for editing.
- (void)voiceSearchCoordinator:(ChromeCoordinator*)coordinator
        didRequestEditingQuery:(NSString*)query;

// Called when the Voice Plate should be dismissed without a query. The owner is
// expected to stop `coordinator`, which dismisses it.
- (void)voiceSearchCoordinatorDidFinish:(ChromeCoordinator*)coordinator;

// Called when the user asks to try Voice Search again after `coordinator` was
// stopped following `-voiceSearchCoordinator:didRequestEditingQuery:`. The
// owner is expected to start a new Voice Search from the same entry point.
- (void)voiceSearchCoordinatorDidRequestRetry:(ChromeCoordinator*)coordinator;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_COORDINATOR_DELEGATE_H_
