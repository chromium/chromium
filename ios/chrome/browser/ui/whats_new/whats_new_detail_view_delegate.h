// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_DELEGATE_H_

@class WhatsNewDetailViewController;

// Delegate protocol to handle communication from the detail view to the
// parent coordinator.
@protocol WhatsNewDetailViewDelegate

// Invoked to request the delegate to dismiss the detail view.
- (void)dismissWhatsNewDetailView:
    (WhatsNewDetailViewController*)whatsNewDetailViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_DELEGATE_H_
