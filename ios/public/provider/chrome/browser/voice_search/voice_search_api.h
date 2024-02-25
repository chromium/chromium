// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_API_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/voice/model/voice_search_language.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_controller.h"

class Browser;

namespace ios {
namespace provider {

// Returns whether Voice Search is enabled.
bool IsVoiceSearchEnabled();

// Returns the list of available Voice Search languages.
NSArray<VoiceSearchLanguage*>* GetAvailableLanguages();

// Creates a new VoiceSearchController instance.
id<VoiceSearchController> CreateVoiceSearchController(Browser* browser);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_API_H_
