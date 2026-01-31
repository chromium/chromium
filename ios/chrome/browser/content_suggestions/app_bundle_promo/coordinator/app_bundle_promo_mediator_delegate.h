// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_DELEGATE_H_

#import "base/ios/block_types.h"

enum class ContentSuggestionsModuleType;

// Handles App Bundle promo module events.
@protocol AppBundlePromoMediatorDelegate

// Indicates to the receiver that the App Bundle promo module should be removed.
// The `completion` is called after the removal is finished.
- (void)removeAppBundlePromoModuleWithCompletion:(ProceduralBlock)completion;

// Logs a user Magic Stack engagement for module `type`.
- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_APP_BUNDLE_PROMO_COORDINATOR_APP_BUNDLE_PROMO_MEDIATOR_DELEGATE_H_
