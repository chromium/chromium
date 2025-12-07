// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCANNER_UI_BUNDLED_SCANNER_ALERTS_H_
#define IOS_CHROME_BROWSER_SCANNER_UI_BUNDLED_SCANNER_ALERTS_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/scanner/ui_bundled/camera_controller.h"

namespace scanner {

// Block type that takes a UIAlertAction. Blocks of this type will be called
// when the Cancel button of a UIAlertView is pressed.
typedef void (^CancelAlertAction)(UIAlertAction* alertAction);

// Returns a dialog to be displayed when the camera state is `state`.
// `cancelBlock` is executed when the button to close the dialog is tapped. If
// `cancelBlock` is nil, the dialog is dismissed on cancel.
UIAlertController* DialogForCameraState(CameraState state,
                                        CancelAlertAction cancelBlock);

}  // namespace scanner

#endif  // IOS_CHROME_BROWSER_SCANNER_UI_BUNDLED_SCANNER_ALERTS_H_
