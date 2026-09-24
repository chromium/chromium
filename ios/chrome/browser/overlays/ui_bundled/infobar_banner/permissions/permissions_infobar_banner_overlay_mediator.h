// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_PERMISSIONS_PERMISSIONS_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_PERMISSIONS_PERMISSIONS_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"

// Mediator that configures an infobar banner for the permissions infobar.
@interface PermissionsBannerOverlayMediator : InfobarBannerOverlayMediator

// Handler for page action menu commands.
@property(nonatomic, weak) id<PageActionMenuCommands> pageActionMenuHandler;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_PERMISSIONS_PERMISSIONS_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
