// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/text_view_util.h"

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

// TODO(crbug.com/1335912): On iOS 16, EG is unable to tap links in
// TextKit2-based UITextViews. Fall back to TextKit1 until this issue
// is resolved.
// Creates a UITextView with TextKit1 by disabling TextKit2.
UITextView* CreateUITextViewWithTextKit1() {
  if (@available(iOS 16, *))
    return [UITextView textViewUsingTextLayoutManager:NO];
  return [[UITextView alloc] init];
}
