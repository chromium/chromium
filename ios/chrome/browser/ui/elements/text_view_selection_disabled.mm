// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/text_view_selection_disabled.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if !defined(__IPHONE_16_0) || __IPHONE_OS_VERSION_MAX_ALLOWED < __IPHONE_16_0
@interface UITextView (TextKit)
// Forward declare iOS 16 `+textViewUsingTextLayoutManager` on iOS 15 SDK
// builds.
+ (instancetype)textViewUsingTextLayoutManager:(BOOL)usingTextLayoutManager;
@end
#endif

@implementation TextViewSelectionDisabled

+ (TextViewSelectionDisabled*)textView {
  // TODO(crbug.com/1335912): On iOS 16, EG is unable to tap links in
  // TextKit2-based UITextViews. Fall back to TextKit1 until this issue
  // is resolved.
  if (@available(iOS 16, *))
    return [TextViewSelectionDisabled textViewUsingTextLayoutManager:NO];
  return [[TextViewSelectionDisabled alloc] init];
}

- (BOOL)canBecomeFirstResponder {
  return NO;
}

@end
