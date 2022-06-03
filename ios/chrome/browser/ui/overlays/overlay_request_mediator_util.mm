// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_request_mediator_util.h"

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

std::unique_ptr<OverlayRequestSupport> CreateAggregateSupportForMediators(
    NSArray<Class>* mediator_classes) {
  std::vector<const OverlayRequestSupport*> supports(mediator_classes.count);
  for (size_t index = 0; index < supports.size(); ++index) {
    Class mediator_class = mediator_classes[index];
    DCHECK([mediator_class isSubclassOfClass:[OverlayRequestMediator class]]);
    supports[index] = [mediator_class requestSupport];
    DCHECK(supports[index]);
  }
  return std::make_unique<OverlayRequestSupport>(supports);
}

OverlayRequestMediator* GetMediatorForRequest(NSArray<Class>* mediator_classes,
                                              OverlayRequest* request) {
  for (Class mediator_class in mediator_classes) {
    DCHECK([mediator_class isSubclassOfClass:[OverlayRequestMediator class]]);
    if ([mediator_class requestSupport]->IsRequestSupported(request))
      return [[mediator_class alloc] initWithRequest:request];
  }
  return nil;
}
