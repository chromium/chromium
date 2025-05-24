// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/instruction_view/constants.h"

NSString* InstructionViewRowAccessibilityIdentifier(int index) {
  return [NSString
      stringWithFormat:@"InstructionViewRowAccessibilityIdentifier%i", index];
}
