// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/print/print_controller.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrintController () <UIPrintInteractionControllerDelegate>
@end

@implementation PrintController

#pragma mark - Public Methods

- (void)printView:(UIView*)view withTitle:(NSString*)title {
  UIPrintPageRenderer* renderer = [[UIPrintPageRenderer alloc] init];
  [renderer addPrintFormatter:[view viewPrintFormatter]
        startingAtPageAtIndex:0];

  [self printRenderer:renderer orItem:nil withTitle:title];
}

- (void)printImage:(UIImage*)image title:(NSString*)title {
  [self printRenderer:nil orItem:image withTitle:title];
}

- (void)dismissAnimated:(BOOL)animated {
  [[UIPrintInteractionController sharedPrintController]
      dismissAnimated:animated];
}

#pragma mark - WebStatePrinter

- (void)printWebState:(web::WebState*)webState {
  [self printView:webState->GetView()
        withTitle:tab_util::GetTabTitle(webState)];
}

#pragma mark - UIPrintInteractionControllerDelegate
- (UIViewController*)printInteractionControllerParentViewController:
    (UIPrintInteractionController*)printInteractionController {
  return [self.delegate baseViewControllerForPrintPreview];
}

#pragma mark - Private methods

// Utility method to print either a renderer or a printable item (as documented
// in UIPrintInteractionController printingItem).
// Exactly one of |renderer| and |item| must be not nil.
- (void)printRenderer:(UIPrintPageRenderer*)renderer
               orItem:(id)item
            withTitle:(NSString*)title {
  // Only one item must be passed.
  DCHECK_EQ((renderer ? 1 : 0) + (item ? 1 : 0), 1);
  DCHECK([self.delegate baseViewControllerForPrintPreview]);
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
      completionHandler:^(
          UIPrintInteractionController* printInteractionController,
          BOOL completed, NSError* error) {
        if (error)
          DLOG(ERROR) << "Air printing error: "
                      << base::SysNSStringToUTF8(error.description);
      }];
}

@end
