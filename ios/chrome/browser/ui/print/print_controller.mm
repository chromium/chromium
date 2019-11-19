// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/print/print_controller.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PrintController

#pragma mark - Public Methods

- (void)printView:(UIView*)view withTitle:(NSString*)title {
  base::RecordAction(base::UserMetricsAction("MobilePrintMenuAirPrint"));
  UIPrintInteractionController* printInteractionController =
      [UIPrintInteractionController sharedPrintController];
  UIPrintInfo* printInfo = [UIPrintInfo printInfo];
  printInfo.outputType = UIPrintInfoOutputGeneral;
  printInfo.jobName = title;
  printInteractionController.printInfo = printInfo;

  UIPrintPageRenderer* renderer = [[UIPrintPageRenderer alloc] init];
  [renderer addPrintFormatter:[view viewPrintFormatter]
        startingAtPageAtIndex:0];
  printInteractionController.printPageRenderer = renderer;

  [printInteractionController
        presentAnimated:YES
      completionHandler:^(
          UIPrintInteractionController* printInteractionController,
          BOOL completed, NSError* error) {
        if (error)
          DLOG(ERROR) << "Air printing error: " << error.description;
      }];
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

@end
