// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/print/print_controller.h"

#import "base/logging.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrintController () <UIPrintInteractionControllerDelegate>
// The view controller the system print dialog should be presented from if not
// specified in the print* command.
@property(nonatomic, weak) UIViewController* defaultBaseViewController;

// The view controller the system print dialog should be presented from.
// This can be passed in the print* method or `defaultBaseViewController` will
// be used.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

@implementation PrintController

#pragma mark - Public Methods

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    self.defaultBaseViewController = baseViewController;
  }
  return self;
}

- (void)printView:(UIView*)view
             withTitle:(NSString*)title
    baseViewController:baseViewController {
  UIPrintPageRenderer* renderer = [[UIPrintPageRenderer alloc] init];
  [renderer addPrintFormatter:[view viewPrintFormatter]
        startingAtPageAtIndex:0];

  [self printRenderer:renderer
                  orItem:nil
               withTitle:title
      baseViewController:baseViewController];
}

- (void)printImage:(UIImage*)image
                 title:(NSString*)title
    baseViewController:baseViewController {
  [self printRenderer:nil
                  orItem:image
               withTitle:title
      baseViewController:baseViewController];
}

- (void)dismissAnimated:(BOOL)animated {
  [[UIPrintInteractionController sharedPrintController]
      dismissAnimated:animated];
}

#pragma mark - WebStatePrinter

- (void)printWebState:(web::WebState*)webState {
  [self printWebState:webState
      baseViewController:self.defaultBaseViewController];
}

- (void)printWebState:(web::WebState*)webState
    baseViewController:baseViewController {
  [self printView:webState->GetView()
               withTitle:tab_util::GetTabTitle(webState)
      baseViewController:baseViewController];
}

#pragma mark - UIPrintInteractionControllerDelegate
- (UIViewController*)printInteractionControllerParentViewController:
    (UIPrintInteractionController*)printInteractionController {
  return self.baseViewController;
}

#pragma mark - Private methods

// Utility method to print either a renderer or a printable item (as documented
// in UIPrintInteractionController printingItem).
// Exactly one of `renderer` and `item` must be not nil.
- (void)printRenderer:(UIPrintPageRenderer*)renderer
                orItem:(id)item
             withTitle:(NSString*)title
    baseViewController:(UIViewController*)baseViewController {
  // Only one item must be passed.
  DCHECK_EQ((renderer ? 1 : 0) + (item ? 1 : 0), 1);
  DCHECK(baseViewController);
  self.baseViewController = baseViewController;
  base::RecordAction(base::UserMetricsAction("MobilePrintMenuAirPrint"));
  UIPrintInteractionController* printInteractionController =
      [UIPrintInteractionController sharedPrintController];
  printInteractionController.delegate = self;

  UIPrintInfo* printInfo = [UIPrintInfo printInfo];
  printInfo.outputType = UIPrintInfoOutputGeneral;
  printInfo.jobName = title;
  printInteractionController.printInfo = printInfo;

  printInteractionController.printPageRenderer = renderer;
  printInteractionController.printingItem = item;

  [printInteractionController
        presentAnimated:YES
      completionHandler:^(UIPrintInteractionController* controller,
                          BOOL completed, NSError* error) {
        if (error)
          DLOG(ERROR) << "Air printing error: "
                      << base::SysNSStringToUTF8(error.description);
      }];
}

@end
