// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_CONTEXTUAL_CUE_CONTEXTUAL_CUE_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_CONTEXTUAL_CUE_CONTEXTUAL_CUE_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"

@protocol SettingsCommands;

// Mediator that configures an infobar banner for a contextual cue infobar.
@interface ContextualCueInfobarBannerOverlayMediator
    : InfobarBannerOverlayMediator

// Handler for `SettingsCommands`.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_CONTEXTUAL_CUE_CONTEXTUAL_CUE_INFOBAR_BANNER_OVERLAY_MEDIATOR_H_
