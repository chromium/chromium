// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/reading_list/reading_list_infobar_modal_overlay_coordinator.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/reading_list_modal_overlay_request_config.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/reading_list/reading_list_infobar_modal_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ReadingListInfobarModalOverlayCoordinator ()
// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, strong) OverlayRequestMediator* modalMediator;
// The modal view controller managed by this coordinator.
@property(nonatomic, strong) UIViewController* modalViewController;
// The request's config to allow the factory class to know to use this
// coordinator class for ReadingListInfobarModalOverlayRequestConfig.
@property(nonatomic, assign, readonly)
    ReadingListInfobarModalOverlayRequestConfig* config;
@end

@implementation ReadingListInfobarModalOverlayCoordinator

#pragma mark - Accessors

- (ReadingListInfobarModalOverlayRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<ReadingListInfobarModalOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - Public

+ (const OverlayRequestSupport*)requestSupport {
  return ReadingListInfobarModalOverlayRequestConfig::RequestSupport();
}

#pragma mark - Private

- (ReadingListInfobarModalOverlayMediator*)readingListModalOverlayMediator {
  return base::mac::ObjCCastStrict<ReadingListInfobarModalOverlayMediator>(
      self.modalMediator);
}

@end

@implementation ReadingListInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);
  ReadingListInfobarModalOverlayMediator* modalMediator =
      [[ReadingListInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  self.modalMediator = modalMediator;
}

- (void)resetModal {
  DCHECK(self.modalMediator);
  self.modalMediator = nil;
}

@end
