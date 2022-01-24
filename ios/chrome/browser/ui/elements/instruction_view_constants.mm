// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/instruction_view_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace instruction_view {
NSString* const kInstructionViewBeginBoldTag = @"BEGIN_BOLD[ \t]*";
NSString* const kInstructionViewEndBoldTag = @"[ \t]*END_BOLD";
}  // instruction_view
