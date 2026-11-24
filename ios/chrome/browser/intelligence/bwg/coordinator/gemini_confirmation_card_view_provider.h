// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONFIRMATION_CARD_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONFIRMATION_CARD_VIEW_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_view_provider.h"

// Renders the confirmation card Gemini shows before it starts a browsing task
// on the user's behalf. When the card's button is tapped, it sends a message
// in their conversation that triggers the start of the task. Vended views may
// outlive the provider.
@interface GeminiConfirmationCardViewProvider
    : NSObject <GeminiCustomUIViewProvider>
@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONFIRMATION_CARD_VIEW_PROVIDER_H_
