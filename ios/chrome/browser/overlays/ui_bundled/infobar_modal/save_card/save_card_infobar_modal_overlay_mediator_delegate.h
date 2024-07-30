// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_DELEGATE_H_

class GURL;

// Allows the SaveCardInfobarModalOverlayMediator to communicate to present
// information.
@protocol SaveCardInfobarModalOverlayMediatorDelegate <NSObject>

// Sets the URL to load when it finishes dismissing the modal UI.
- (void)pendingURLToLoad:(GURL)URL;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_MEDIATOR_DELEGATE_H_
