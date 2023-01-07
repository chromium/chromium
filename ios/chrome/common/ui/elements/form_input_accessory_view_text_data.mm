// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryViewTextData ()

@property(nonatomic, readwrite, copy) NSString* closeButtonTitle;
@property(nonatomic, readwrite, copy) NSString* closeButtonAccessibilityLabel;
@property(nonatomic, readwrite, copy) NSString* nextButtonAccessibilityLabel;
@property(nonatomic, readwrite, copy)
    NSString* previousButtonAccessibilityLabel;

@end

@implementation FormInputAccessoryViewTextData

- (instancetype)initWithCloseButtonTitle:(NSString*)closeButtonTitle
           closeButtonAccessibilityLabel:
               (NSString*)closeButtonAccessibilityLabel
            nextButtonAccessibilityLabel:(NSString*)nextButtonAccessibilityLabel
        previousButtonAccessibilityLabel:
            (NSString*)previousButtonAccessibilityLabel {
  if (self = [super init]) {
    _closeButtonTitle = closeButtonTitle;
    _closeButtonAccessibilityLabel = closeButtonAccessibilityLabel;
    _nextButtonAccessibilityLabel = nextButtonAccessibilityLabel;
    _previousButtonAccessibilityLabel = previousButtonAccessibilityLabel;
  }
  return self;
}

@end
