// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_API_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/voice/model/voice_search_language.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_controller.h"

class Browser;
@class ChromeCoordinator;
@class UIViewController;
@protocol VoiceSearchCoordinatorDelegate;

namespace ios {
namespace provider {

// Returns whether Voice Search is enabled.
bool IsVoiceSearchEnabled();

// Returns the list of available Voice Search languages.
NSArray<VoiceSearchLanguage*>* GetAvailableLanguages();

// Creates a new VoiceSearchController instance.
// TODO(crbug.com/549858749): Remove once the legacy Voice Search UI is removed.
id<VoiceSearchController> CreateVoiceSearchController(Browser* browser);

// Creates a coordinator presenting the Voice Plate on `base_view_controller`.
// The outcome of the session is reported to `delegate`.
ChromeCoordinator* CreateVoiceSearchCoordinator(
    UIViewController* base_view_controller,
    Browser* browser,
    id<VoiceSearchCoordinatorDelegate> delegate);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_VOICE_SEARCH_VOICE_SEARCH_API_H_
