// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/test/data_protection_app_interface.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_protection/public/pref_names.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"

namespace {

// Recursively checks if `view` or any subview is a visible WatermarkView.
BOOL ContainsVisibleWatermarkView(UIView* view) {
  if ([view isKindOfClass:[WatermarkView class]] && !view.hidden &&
      view.alpha > 0.01) {
    return YES;
  }
  for (UIView* subview in view.subviews) {
    if (ContainsVisibleWatermarkView(subview)) {
      return YES;
    }
  }
  return NO;
}

}  // namespace

@implementation DataProtectionAppInterface

+ (void)setWatermarkText:(NSString*)watermarkText {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  CHECK(web_state);
  DataProtectionTabHelper* helper =
      DataProtectionTabHelper::FromWebState(web_state);
  CHECK(helper);
  helper->SetWatermarkTextForTesting(base::SysNSStringToUTF8(watermarkText));
}

+ (BOOL)canGenerateFullPageScreenshotPDF {
  SceneState* sceneState = chrome_test_util::GetForegroundActiveScene();
  CHECK(sceneState);
  UIScreenshotService* screenshotService = sceneState.scene.screenshotService;
  CHECK(screenshotService);
  id<UIScreenshotServiceDelegate> delegate = screenshotService.delegate;
  CHECK(delegate);

  __block bool callbackCalled = false;
  __block BOOL hasPDFData = NO;
  [delegate screenshotService:screenshotService
      generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                NSInteger indexOfCurrentPage,
                                                CGRect rectInCurrentPage) {
        hasPDFData = PDFData.length > 0;
        callbackCalled = true;
      }];

  if (!base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForActionTimeout, ^{
            return callbackCalled;
          })) {
    return NO;
  }
  return hasPDFData;
}

@end
