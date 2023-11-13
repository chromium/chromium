// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/ui/permissions/permissions_delegate.h"

@protocol InfobarModalDelegate;
@protocol PermissionsConsumer;

// Mediator that configures the modal UI for permissions infobar.
@interface PermissionsInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarModalDelegate, PermissionsDelegate>

// Consumer that is configured by this mediator.
@property(nonatomic, weak) id<PermissionsConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
