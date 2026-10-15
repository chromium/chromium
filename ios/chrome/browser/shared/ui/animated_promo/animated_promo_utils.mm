// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"

#import <map>
#import <string>
#import <tuple>

#import "base/check_op.h"
#import "base/not_fatal_until.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "skia/ext/skia_utils_ios.h"

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

void ConfigureAnimationCustomColor(id<LottieAnimation> animation,
                                   NSString* key,
                                   SkColor light_color,
                                   SkColor dark_color) {
  ConfigureAnimationCustomColor(animation, key,
                                skia::UIColorFromSkColor(light_color),
                                skia::UIColorFromSkColor(dark_color));
}

void ConfigureAnimationCustomColors(
    id<LottieAnimation> animation,
    NSDictionary<NSString*, UIColor*>* light_color_provider,
    NSDictionary<NSString*, UIColor*>* dark_color_provider) {
  CHECK_EQ(light_color_provider.count, dark_color_provider.count,
           base::NotFatalUntil::M161);

  // ConfigureAnimationCustomColor uses a regex with a leading wildcard, meaning
  // that different keypaths sharing the same suffix could be grouped together.
  // To prevent this, keys must be sorted by length (ascending) so that shorter
  // keys are processed first.
  NSArray<NSString*>* sortedKeys = [light_color_provider.allKeys
      sortedArrayUsingComparator:^NSComparisonResult(NSString* key1,
                                                     NSString* key2) {
        if (key1.length < key2.length) {
          return NSOrderedAscending;
        }
        if (key1.length > key2.length) {
          return NSOrderedDescending;
        }
        return [key1 compare:key2];
      }];

  for (NSString* keypath in sortedKeys) {
    UIColor* lightColor = light_color_provider[keypath];
    UIColor* darkColor = dark_color_provider[keypath];
    ConfigureAnimationCustomColor(animation, keypath, lightColor, darkColor);
  }
}
