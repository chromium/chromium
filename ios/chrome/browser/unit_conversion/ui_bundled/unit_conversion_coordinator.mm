// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_coordinator.h"

#import <algorithm>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/unit_conversion/model/unit_conversion_service.h"
#import "ios/chrome/browser/unit_conversion/model/unit_conversion_service_factory.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_mediator.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_view_controller.h"
#import "ios/web/public/web_state.h"

namespace {

// Popover source rect dimensions.
const CGFloat kPopOverSourceRectWidth = 1;
const CGFloat kPopOverSourceRectHeight = 1;

// The height offset to add to the half sheet detent's height.
const CGFloat kHalfSheetDetentHeightOffset = 40;

// Sets a custom radius for the half sheet.
CGFloat const kHalfSheetCornerRadius = 13;

}  // namespace

@interface UnitConversionCoordinator () <
    UIAdaptivePresentationControllerDelegate>

// The view controller managed by this coordinator.
@property(nonatomic, strong) UnitConversionViewController* viewController;

@end

@implementation UnitConversionCoordinator {
  // Mediator to handle the units updates and conversion.
  UnitConversionMediator* _mediator;

  // The detected unit.
  NSUnit* _sourceUnit;

  // The detected unit value.
  double _sourceUnitValue;

  // The user's tap/long press location.
  CGPoint _location;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                sourceUnit:(NSUnit*)sourceUnit
                           sourceUnitValue:(double)sourceUnitValue
                                  location:(CGPoint)location {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _sourceUnit = sourceUnit;
    _sourceUnitValue = sourceUnitValue;
    _location = location;
  }
  return self;
}

- (void)start {
  // Init the keyed service to track the changes of the target unit and pass it
  // to the mediator.
  UnitConversionService* service =
      UnitConversionServiceFactory::GetForProfile(self.profile);
  _mediator = [[UnitConversionMediator alloc] initWithService:service];
  _viewController = [[UnitConversionViewController alloc]
      initWithSourceUnit:_sourceUnit
              targetUnit:service->GetDefaultTargetFromUnit(_sourceUnit)
               unitValue:_sourceUnitValue];
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  _viewController.delegate = self;

  [self presentUnitConversionViewController];
}

- (void)stop {
  [_mediator reportMetrics];
  [_mediator shutdown];
  _mediator = nil;
  [self dismissViewController];
}

#pragma mark - UnitConversionViewControllerDelegate

- (void)didTapCloseUnitConversionController:
    (UnitConversionViewController*)viewController {
  [self stop];
}

- (void)didTapReportIssueUnitConversionController:
    (UnitConversionViewController*)viewController {
  DCHECK(viewController == _viewController);
  id<SceneCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [handler
      showReportAnIssueFromViewController:_viewController
                                   sender:UserFeedbackSender::UnitConversion];
}

#pragma mark - Private

// Presents the UnitConversionCoordinator's view controller and adapt the
// presentation based on the device (popover for ipad, half sheet for iphone)
- (void)presentUnitConversionViewController {
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  navigationController.modalPresentationStyle = UIModalPresentationPopover;
  UIPopoverPresentationController* popover =
      navigationController.popoverPresentationController;
  popover.delegate = _viewController;
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  UIView* sourceView = activeWebState ? activeWebState->GetView() : nil;

  popover.sourceView = sourceView ?: self.baseViewController.view;

  CGPoint location = _location;
  if (sourceView) {
    // Convert the location from the browser view's coordinate system to the
    // WebState view's coordinate system, and clamp it to the bounds of the
    // WebState view.
    location = [self.baseViewController.view convertPoint:_location
                                                   toView:sourceView];
    CGRect bounds = sourceView.bounds;
    location.x =
        std::clamp(location.x, CGRectGetMinX(bounds), CGRectGetMaxX(bounds));
    location.y =
        std::clamp(location.y, CGRectGetMinY(bounds), CGRectGetMaxY(bounds));
  }

  popover.sourceRect =
      CGRectMake(location.x, location.y, kPopOverSourceRectWidth,
                 kPopOverSourceRectHeight);
  popover.permittedArrowDirections =
      UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;
  UISheetPresentationController* sheetPresentationController =
      popover.adaptiveSheetPresentationController;
  sheetPresentationController.delegate = _viewController;
  sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
  sheetPresentationController.preferredCornerRadius = kHalfSheetCornerRadius;

  __weak UnitConversionCoordinator* weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    CGFloat sheetHeight = weakSelf.viewController.preferredContentSize.height +
                          kHalfSheetDetentHeightOffset;
    BOOL tooLarge = (sheetHeight > context.maximumDetentValue);
    return tooLarge ? context.maximumDetentValue : sheetHeight;
  };

  UISheetPresentationControllerDetent* customDetent =
      [UISheetPresentationControllerDetent customDetentWithIdentifier:nil
                                                             resolver:resolver];

  sheetPresentationController.detents = @[ customDetent ];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

// Dismisses the UnitConversionCoordinator's view controller.
- (void)dismissViewController {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController.delegate = nil;
  _viewController = nil;
}

@end
