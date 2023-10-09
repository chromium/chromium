// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VOICE_MODEL_TEXT_TO_SPEECH_PARSER_H_
#define IOS_CHROME_BROWSER_VOICE_MODEL_TEXT_TO_SPEECH_PARSER_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}

// Extracts TTS audio data from `webState`'s HTML using the start and end tags
// for Google Voice Search result pages.
typedef void (^TextToSpeechCompletion)(NSData*);
void ExtractVoiceSearchAudioDataFromWebState(web::WebState* webState,
                                             TextToSpeechCompletion completion);

#endif  // IOS_CHROME_BROWSER_VOICE_MODEL_TEXT_TO_SPEECH_PARSER_H_
