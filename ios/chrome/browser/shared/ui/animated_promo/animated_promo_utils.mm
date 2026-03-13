// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"

#import <map>
#import <string>
#import <tuple>

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

void ConfigureAnimationSemanticColor(id<LottieAnimation> animation,
                                     NSString* key,
                                     NSString* color_name) {
  NSString* keypath = [NSString stringWithFormat:@"**%@.**.Color", key];
  [animation setColorValue:[UIColor colorNamed:color_name] forKeypath:keypath];
}

void ConfigureAnimationCustomColor(id<LottieAnimation> animation,
                                   NSString* key,
                                   UIColor* light_color,
                                   UIColor* dark_color) {
  UIColor* selected_color = [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        return (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark)
                   ? dark_color
                   : light_color;
      }];
  NSString* keypath = [NSString stringWithFormat:@"**%@.**.Color", key];
  [animation setColorValue:selected_color forKeypath:keypath];
}
