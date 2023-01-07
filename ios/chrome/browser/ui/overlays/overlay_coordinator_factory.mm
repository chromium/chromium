// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory.h"

#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_supported_overlay_coordinator_classes.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_supported_overlay_coordinator_classes.h"
#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory+initialization.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/web_content_area_supported_overlay_coordinator_classes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OverlayRequestCoordinatorFactory ()
// The Browser passed on initialization.
@property(nonatomic, readonly) Browser* browser;
// The OverlayRequestCoordinator subclasses that are supported at the modality
// associated with this coordinator factory.
@property(nonatomic, readonly)
    NSArray<Class>* supportedOverlayRequestCoordinatorClasses;
@end

@implementation OverlayRequestCoordinatorFactory

+ (instancetype)factoryForBrowser:(Browser*)browser
                         modality:(OverlayModality)modality {
  DCHECK(browser);
  NSArray<Class>* supportedCoordinatorClasses = @[];
  switch (modality) {
    case OverlayModality::kTesting:
      // Use TestOverlayRequestCoordinatorFactory to create factories for
      // OverlayModality::kTesting.
      // TODO(crbug.com/1056837): Remove requirement once modalities are
      // converted to no longer use enums.
      NOTREACHED();
      break;
    case OverlayModality::kWebContentArea:
      supportedCoordinatorClasses =
          web_content_area::GetSupportedOverlayCoordinatorClasses();
      break;
    case OverlayModality::kInfobarBanner:
      supportedCoordinatorClasses =
          infobar_banner::GetSupportedOverlayCoordinatorClasses();
      break;
    case OverlayModality::kInfobarModal:
      supportedCoordinatorClasses =
          infobar_modal::GetSupportedOverlayCoordinatorClasses();
      break;
  }
  return [[self alloc] initWithBrowser:browser
      supportedOverlayRequestCoordinatorClasses:supportedCoordinatorClasses];
}

- (BOOL)coordinatorForRequestUsesChildViewController:(OverlayRequest*)request {
  return [[self coordinatorClassForRequest:request]
      showsOverlayUsingChildViewController];
}

- (OverlayRequestCoordinator*)
    newCoordinatorForRequest:(OverlayRequest*)request
                    delegate:(OverlayRequestCoordinatorDelegate*)delegate
          baseViewController:(UIViewController*)baseViewController {
  return [[[self coordinatorClassForRequest:request] alloc]
      initWithBaseViewController:baseViewController
                         browser:self.browser
                         request:request
                        delegate:delegate];
}

#pragma mark - Helpers

// Returns the OverlayRequestCoordinator subclass responsible for showing
// `request`'s overlay UI.
- (Class)coordinatorClassForRequest:(OverlayRequest*)request {
  NSArray<Class>* supportedClasses =
      self.supportedOverlayRequestCoordinatorClasses;
  for (Class coordinatorClass in supportedClasses) {
    if ([coordinatorClass requestSupport]->IsRequestSupported(request))
      return coordinatorClass;
  }
  NOTREACHED() << "Received unsupported request type.";
  return nil;
}

@end

@implementation OverlayRequestCoordinatorFactory (Initialization)

- (instancetype)initWithBrowser:(Browser*)browser
    supportedOverlayRequestCoordinatorClasses:
        (NSArray<Class>*)supportedOverlayClasses {
  if (self = [super init]) {
    _browser = browser;
    DCHECK(_browser);
    _supportedOverlayRequestCoordinatorClasses = supportedOverlayClasses;
    DCHECK(_supportedOverlayRequestCoordinatorClasses.count);
  }
  return self;
}

@end
