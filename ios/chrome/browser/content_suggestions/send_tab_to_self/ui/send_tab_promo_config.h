// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_CONFIG_H_

#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view_config.h"

@protocol SendTabPromoAudience;

// Item containing the configurations for the Send Tab Promo Module view.
@interface SendTabPromoConfig
    : StandaloneModuleViewConfig <StandaloneModuleViewTapDelegate>
// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)
// The object that should handle user events.
@property(nonatomic, weak) id<SendTabPromoAudience> audience;
// LINT.ThenChange(send_tab_promo_config.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_CONFIG_H_
