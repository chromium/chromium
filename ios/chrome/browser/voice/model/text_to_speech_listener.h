// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_TEXT_TO_SPEECH_LISTENER_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_TEXT_TO_SPEECH_LISTENER_H_

#import <Foundation/Foundation.h>

class GURL;
namespace web {
class WebState;
}

@protocol TextToSpeechListenerDelegate;

// Class that listens for page loads on a WebState and extracts TTS data.
@interface TextToSpeechListener : NSObject

// The WebState passed on initialization.
@property(nonatomic, readonly) web::WebState* webState;

// Designated initializer.
- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:(id<TextToSpeechListenerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@protocol TextToSpeechListenerDelegate <NSObject>

// Called by `listener` when TTS audio data has been extracted from its
// WebState.  If a page load was encountered that was not a Voice Search SRP,
// this function is called with a nil `result`.
- (void)textToSpeechListener:(TextToSpeechListener*)listener
            didReceiveResult:(NSData*)result;

// Called by `listener` after its WebState is destroyed.
- (void)textToSpeechListenerWebStateWasDestroyed:
    (TextToSpeechListener*)listener;

// Called by `listener` to determine whether `URL` is a Voice Search SRP with
// Text-To-Speech data.
- (BOOL)shouldTextToSpeechListener:(TextToSpeechListener*)listener
                  parseDataFromURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_TEXT_TO_SPEECH_LISTENER_H_
