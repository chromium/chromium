// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_coordinator.h"

#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface PasswordSuggestionBottomSheetCoordinator ()

// This mediator is used to fetch data related to the bottom sheet.
@property(nonatomic, strong) PasswordSuggestionBottomSheetMediator* mediator;

// This view controller is used to display the bottom sheet.
@property(nonatomic, strong)
    PasswordSuggestionBottomSheetViewController* viewController;

@end

@implementation PasswordSuggestionBottomSheetCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        params:(const autofill::FormActivityParams&)params
                      delegate:(id<PasswordControllerDelegate>)delegate {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    self.viewController = [[PasswordSuggestionBottomSheetViewController alloc]
        initWithFaviconLoader:IOSChromeFaviconLoaderFactory::GetForBrowserState(
                                  browser->GetBrowserState())];
    self.mediator = [[PasswordSuggestionBottomSheetMediator alloc]
        initWithWebStateList:browser->GetWebStateList()
                      params:params];
    self.viewController.delegate = self.mediator;

    self.mediator.consumer = self.viewController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (@available(iOS 15, *)) {
    self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        self.viewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  }

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

@end
