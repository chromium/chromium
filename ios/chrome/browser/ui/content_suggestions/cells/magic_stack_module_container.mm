// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The corner radius of this container.
const float kCornerRadius = 16;

// The width of the modules.
const int kModuleWidthCompact = 343;
const int kModuleWidthRegular = 382;

}  // namespace

@interface MagicStackModuleContainer ()

// The type of this container.
@property(nonatomic, assign) ContentSuggestionsModuleType type;

@end

@implementation MagicStackModuleContainer

- (instancetype)initWithType:(ContentSuggestionsModuleType)type {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _type = type;
    self.layer.cornerRadius = kCornerRadius;
    self.backgroundColor = [UIColor colorNamed:kGrey100Color];
  }
  return self;
}

+ (CGFloat)moduleWidthForHorizontalTraitCollection:
    (UITraitCollection*)traitCollection {
  return traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular
             ? kModuleWidthRegular
             : kModuleWidthCompact;
}

- (NSString*)titleString {
  switch (self.type) {
    case ContentSuggestionsModuleType::kShortcuts:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_SHORTCUTS_MODULE_TITLE);
    default:
      NOTREACHED();
      return @"";
  }
}

- (CGSize)intrinsicContentSize {
  return CGSizeMake(
      [MagicStackModuleContainer
          moduleWidthForHorizontalTraitCollection:self.traitCollection],
      self.bounds.size.height);
}

@end
