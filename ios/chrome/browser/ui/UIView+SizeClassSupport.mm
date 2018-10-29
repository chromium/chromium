// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the SizeClassIdiom corresponding with |size_class|.
SizeClassIdiom GetSizeClassIdiom(UIUserInterfaceSizeClass size_class) {
  switch (size_class) {
    case UIUserInterfaceSizeClassCompact:
      return COMPACT;
    case UIUserInterfaceSizeClassRegular:
      return REGULAR;
    case UIUserInterfaceSizeClassUnspecified:
      return UNSPECIFIED;
  }
}

// Returns YES if |size_class| is not UIUserInterfaceSizeClassUnspecified.
bool IsSizeClassSpecified(UIUserInterfaceSizeClass size_class) {
  return size_class != UIUserInterfaceSizeClassUnspecified;
}

}  // namespace

@implementation UIView (SizeClassSupport)

- (SizeClassIdiom)cr_widthSizeClass {
  UIWindow* keyWindow = [UIApplication sharedApplication].keyWindow;
  UIUserInterfaceSizeClass sizeClass = self.traitCollection.horizontalSizeClass;
  if (!IsSizeClassSpecified(sizeClass))
    sizeClass = keyWindow.traitCollection.horizontalSizeClass;
  return GetSizeClassIdiom(sizeClass);
}

- (SizeClassIdiom)cr_heightSizeClass {
  UIWindow* keyWindow = [UIApplication sharedApplication].keyWindow;
  UIUserInterfaceSizeClass sizeClass = self.traitCollection.verticalSizeClass;
  if (!IsSizeClassSpecified(sizeClass))
    sizeClass = keyWindow.traitCollection.verticalSizeClass;
  return GetSizeClassIdiom(sizeClass);
}

@end
