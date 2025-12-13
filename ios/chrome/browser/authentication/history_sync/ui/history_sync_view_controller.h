// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_UI_HISTORY_SYNC_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_UI_HISTORY_SYNC_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/authentication/history_sync/ui/history_sync_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

enum class SigninContextStyle;

// View controller for history sync.
@interface HistorySyncViewController
    : PromoStyleViewController <HistorySyncConsumer>

// Designated initializer.
// The `contextStyle` is used to customize content on screen.
- (instancetype)initWithContextStyle:(SigninContextStyle)contextStyle
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_UI_HISTORY_SYNC_VIEW_CONTROLLER_H_
