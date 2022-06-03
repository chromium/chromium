// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/overlays/overlay_request_mediator.h"

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

// Mediator superclass for configuring infobar modal views.
@interface InfobarModalOverlayMediator
    : OverlayRequestMediator <InfobarModalDelegate>
@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
