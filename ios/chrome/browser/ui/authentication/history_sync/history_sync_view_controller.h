// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// View controller for history sync.
@interface HistorySyncViewController
    : PromoStyleViewController <HistorySyncConsumer>

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_VIEW_CONTROLLER_H_
