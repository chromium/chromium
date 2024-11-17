// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/text_view_util.h"

// TODO(crbug.com/40847368): On iOS 16, EG is unable to tap links in
// TextKit2-based UITextViews. Fall back to TextKit1 until this issue
// is resolved.
// Creates a UITextView with TextKit1 by disabling TextKit2.
UITextView* CreateUITextViewWithTextKit1() {
  return [UITextView textViewUsingTextLayoutManager:NO];
}
