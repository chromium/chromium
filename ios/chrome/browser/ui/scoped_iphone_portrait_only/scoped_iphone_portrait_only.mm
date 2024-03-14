// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scoped_iphone_portrait_only/scoped_iphone_portrait_only.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/logging.h"
#import "ios/chrome/browser/ui/scoped_iphone_portrait_only/iphone_portrait_only_manager.h"
#import "ui/base/device_form_factor.h"

ScopedIphonePortraitOnly::ScopedIphonePortraitOnly(
    id<IphonePortraitOnlyManager> manager)
    : manager_(manager) {
  CHECK(manager_);
  CHECK_EQ(ui::GetDeviceFormFactor(), ui::DEVICE_FORM_FACTOR_PHONE)
      << "Form factor: " << ui::GetDeviceFormFactor();
  [manager_ incrementIphonePortraitOnlyCounter];
}

ScopedIphonePortraitOnly::~ScopedIphonePortraitOnly() {
  CHECK(manager_)
      << "Cannot unlock the iPhone portrait only if app state is deallocated.";
  [manager_ decrementIphonePortraitOnlyCounter];
}
