// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/infobars/modals/permissions/infobar_permissions_modal_delegate.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"

@protocol InfobarPermissionsModalConsumer;

// Mediator that configures the modal UI for permissions infobar.
@interface PermissionsInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarPermissionsModalDelegate>

// Consumer that is configured by this mediator.
@property(nonatomic) id<InfobarPermissionsModalConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
