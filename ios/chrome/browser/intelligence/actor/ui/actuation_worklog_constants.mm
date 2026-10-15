// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"

namespace {

// Accessibility identifier prefix for the action buttons of an intervention,
// suffixed with the index of the displayed action.
NSString* const kActionButtonAccessibilityIdentifierPrefix =
    @"ActuationInterventionActionButtonAccessibilityIdentifier-";

}  // namespace

NSString* ActuationInterventionActionButtonAccessibilityIdentifier(
    NSUInteger index) {
  return [NSString stringWithFormat:@"%@%lu",
                                    kActionButtonAccessibilityIdentifierPrefix,
                                    static_cast<unsigned long>(index)];
}
