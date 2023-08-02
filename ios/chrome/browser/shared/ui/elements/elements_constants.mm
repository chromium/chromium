// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"

NSString* const kActivityOverlayViewAccessibilityIdentifier =
    @"ActivityOverlayViewAccessibilityIdentifier";

NSString* InstructionViewRowAccessibilityIdentifier(int index) {
  return [NSString
      stringWithFormat:@"InstructionViewRowAccessibilityIdentifier%i", index];
}
