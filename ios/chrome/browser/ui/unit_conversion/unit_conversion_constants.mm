// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_constants.h"

NSString* const kUnitConversionTableViewIdentifier =
    @"UnitConversionTableViewIdentifier";
NSString* const kSourceUnitLabelIdentifier = @"sourceUnitLabelIdentifier";
NSString* const kSourceUnitMenuButtonIdentifier =
    @"sourceUnitMenuButtonIdentifier";
NSString* const kTargetUnitLabelIdentifier = @"targetUnitLabelIdentifier";
NSString* const kTargetUnitMenuButtonIdentifier =
    @"targetUnitMenuButtonIdentifier";
NSString* const kSourceUnitFieldIdentifier = @"sourceUnitFieldIdentifier";
NSString* const kTargetUnitFieldIdentifier = @"targetUnitFieldIdentifier";

const char kSourceUnitChangeAfterUnitTypeChangeHistogram[] =
    "IOS.UnitConversion.SourceUnitChangeAfterUnitTypeChange";
const char kSourceUnitChangeBeforeUnitTypeChangeHistogram[] =
    "IOS.UnitConversion.SourceUnitChangeBeforeUnitTypeChange";
const char kTargetUnitChangeHistogram[] = "IOS.UnitConversion.TargetUnitChange";
