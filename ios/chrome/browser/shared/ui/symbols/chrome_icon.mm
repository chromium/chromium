// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"

#import <CoreGraphics/CoreGraphics.h>

#import "base/check.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* AccessibilityLabelForIconNamed(NSString* name) {
  if ([name isEqualToString:@"ic_arrow_back"]) {
    return l10n_util::GetNSString(IDS_IOS_ICON_ARROW_BACK);
  }
  if ([name isEqualToString:@"ic_close"]) {
    return l10n_util::GetNSString(IDS_IOS_ICON_CLOSE);
  }
  if ([name isEqualToString:@"ic_info"]) {
    return l10n_util::GetNSString(IDS_IOS_ICON_INFO);
  }
  if ([name isEqualToString:@"ic_search"]) {
    return l10n_util::GetNSString(IDS_IOS_ICON_SEARCH);
  }
  return nil;
}

UIImage* IconNamed(NSString* name) {
  UIImage* image = [UIImage imageNamed:name];
  DCHECK(image);
  image.accessibilityIdentifier = name;
  image.accessibilityLabel = AccessibilityLabelForIconNamed(name);
  return image;
}

// Wraps -[UIImage imageFlippedForRightToLeftLayoutDirection] to also support
// porting accessibility properties.
// TODO(crbug.com/41260431): remove this workaround if Apple fixes
// rdar://26962660
UIImage* ImageFlippedForRightToLeftLayoutDirection(UIImage* image) {
  UIImage* imageFlipped = [image imageFlippedForRightToLeftLayoutDirection];
  imageFlipped.accessibilityIdentifier = image.accessibilityIdentifier;
  imageFlipped.accessibilityLabel = image.accessibilityLabel;
  return imageFlipped;
}

}  // namespace

@implementation ChromeIcon

+ (UIImage*)backIcon {
  return ImageFlippedForRightToLeftLayoutDirection(IconNamed(@"ic_arrow_back"));
}

+ (UIImage*)closeIcon {
  return IconNamed(@"ic_close");
}

+ (UIImage*)infoIcon {
  return IconNamed(@"ic_info");
}

+ (UIImage*)searchIcon {
  return IconNamed(@"ic_search");
}

+ (UIImage*)chevronIcon {
  return ImageFlippedForRightToLeftLayoutDirection(
      IconNamed(@"ic_chevron_right"));
}

+ (UIBarButtonItem*)templateBarButtonItemWithImage:(UIImage*)image
                                            target:(id)target
                                            action:(SEL)action {
  UIImage* templateImage =
      [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIBarButtonItem* barButtonItem =
      [[UIBarButtonItem alloc] initWithImage:templateImage
                                       style:UIBarButtonItemStylePlain
                                      target:target
                                      action:action];
  [barButtonItem setAccessibilityIdentifier:image.accessibilityIdentifier];
  [barButtonItem setAccessibilityLabel:image.accessibilityLabel];
  return barButtonItem;
}

@end
