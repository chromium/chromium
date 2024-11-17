// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_DELEGATE_H_

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

// Delegate protocol to relay information from the Magic Stack standalone
// modules.
@protocol StandaloneModuleDelegate

// Called when the button is tapped on the module of the given type.
- (void)buttonTappedForModuleType:(ContentSuggestionsModuleType)moduleType;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_DELEGATE_H_
