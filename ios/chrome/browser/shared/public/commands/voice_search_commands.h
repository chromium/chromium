// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_VOICE_SEARCH_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_VOICE_SEARCH_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands to control voice search in a browser.
@protocol VoiceSearchCommands <NSObject>

// Preloads voice search in the current BVC. No-op when
// `kVoiceSearchEvolutionPhase1` is enabled.
// TODO(crbug.com/549858749): Remove with the legacy voice search UI.
- (void)preloadVoiceSearch;

// Shows the voice search UI after stopping it on all other browsers in the
// scene.
- (void)startVoiceSearch;

// Stops voice search on this browser. To stop voice search on all browsers in
// a scene, `stopAllVoiceSearch` from `SceneCommands` can be used.
- (void)stopVoiceSearch;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_VOICE_SEARCH_COMMANDS_H_
