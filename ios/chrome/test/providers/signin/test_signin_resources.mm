// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace ios {
namespace provider {

UIImage* GetSigninDefaultAvatar() {
  return ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      CGSizeMake(32, 32), UIColor.whiteColor);
}

}  // namespace provider
}  // namespace ios
