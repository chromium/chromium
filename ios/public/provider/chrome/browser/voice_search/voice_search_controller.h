// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_CONTROLLER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}

@protocol LoadQueryCommands;

// Protocol for interacting with the Voice Search UI.
@protocol VoiceSearchController <NSObject>

// Preloads views and view controllers needed for the Voice Search UI.
- (void)prepareToAppear;

// Starts recognizing and recording process. Will call the delegate method
// upon completion if the recognition succeeds. The Voice Search input UI
// must be presented on `viewController`.
- (void)startRecognitionOnViewController:(UIViewController*)viewController
                                webState:(web::WebState*)webState;

// Dismiss the mic permission help UI if shown.
- (void)dismissMicPermissionHelp;

// Called before the object is destroyed.
- (void)disconnect;

// Dispatcher for this object.
@property(nonatomic, weak) id<LoadQueryCommands> dispatcher;

// Whether or not the Text To Speech user preference is enabled.
@property(nonatomic, readonly) BOOL textToSpeechEnabled;

// Whether Text to Speech is supported for the currently selected Voice Search
// language. If a Voice Search language has not been specified, returns whether
// Text To Speech is supported for the default Voice Search language.
@property(nonatomic, readonly) BOOL textToSpeechSupported;

// Whether the Voice Search UI is visible.
@property(nonatomic, readonly) BOOL visible;

// Whether the Text To Speech audio is currently playing.
@property(nonatomic, readonly) BOOL audioPlaying;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_CONTROLLER_H_
