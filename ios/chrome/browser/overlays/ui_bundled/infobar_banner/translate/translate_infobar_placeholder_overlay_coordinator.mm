// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/translate/translate_infobar_placeholder_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"

@interface TranslateInfobarPlaceholderOverlayCoordinator ()
// The list of supported mediator classes.
@property(class, nonatomic, readonly) NSArray<Class>* supportedMediatorClasses;
@end

@implementation TranslateInfobarPlaceholderOverlayCoordinator

#pragma mark - Accessors

+ (NSArray<Class>*)supportedMediatorClasses {
  return @[];
}

+ (const OverlayRequestSupport*)requestSupport {
  return InfobarBannerPlaceholderRequestConfig::RequestSupport();
}

#pragma mark - OverlayRequestCoordinator

- (void)startAnimated:(BOOL)animated {
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;

  // Mark started as NO before calling dismissal callback to prevent dup
  // stopAnimated: executions.
  self.started = NO;
  // Notify the presentation context that the dismissal has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  self.delegate->OverlayUIDidFinishDismissal(self.request);
}

@end
