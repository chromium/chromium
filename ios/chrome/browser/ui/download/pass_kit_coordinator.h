// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_PASS_KIT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_PASS_KIT_COORDINATOR_H_

#import <PassKit/PassKit.h>

#import "ios/chrome/browser/download/pass_kit_tab_helper_delegate.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace web {
class WebState;
}  // namespace web

// Key of the UMA Download.IOSPresentAddPassesDialogResult histogram. Exposed
// only for testing.
extern const char kUmaPresentAddPassesDialogResult[];

// Enum for the Download.IOSPresentAddPassesDialogResult UMA histogram
// to report the results of the add passes dialog presentation. The presentation
// can be successful or unsuccessful if another view controller is currently
// presented. Unsuccessful presentation is a bug and if the number of
// unsuccessful presentations is high, it means that Chrome has to queue the
// dialogs to present those dialogs for every downloaded pkpass (PassKit file).
// Currently Chrome simply ignores the download if the dialog is already
// presented. Exposed only for testing.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
enum class PresentAddPassesDialogResult {
  // The dialog was sucessesfully presented.
  kSuccessful = 0,
  // The dialog cannot be presented, because another PKAddPassesViewController
  // is already presented.
  kAnotherAddPassesViewControllerIsPresented = 1,
  // The dialog cannot be presented, because another view controller is already
  // presented. Does not include items already counted in the more specific
  // bucket (kAnotherAddPassesViewControllerIsPresented).
  kAnotherViewControllerIsPresented = 2,
  kCount
};

// Coordinates presentation of "Add pkpass UI" and "failed to add pkpass UI".
@interface PassKitCoordinator : ChromeCoordinator<PassKitTabHelperDelegate>

// Must be set before calling |start| method. Set to null when stop method is
// called or web state is destroyed.
@property(nonatomic) web::WebState* webState;

// If the PKPass is a valid pass, then the coordinator will present the "Add
// pkpass UI". Otherwise, the coordinator will present the "failed to add
// pkpass UI". Is set to null when the stop method is called.
@property(nonatomic) PKPass* pass;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_PASS_KIT_COORDINATOR_H_
