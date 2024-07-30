// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator_util.h"

#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator.h"

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
