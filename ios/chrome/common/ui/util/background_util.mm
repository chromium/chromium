// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/background_util.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

UIView* PrimaryBackgroundBlurView() {
  UIVisualEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
  return [[UIVisualEffectView alloc] initWithEffect:blurEffect];
}
