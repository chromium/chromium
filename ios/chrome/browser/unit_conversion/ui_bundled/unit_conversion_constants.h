// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the Unit Conversion View Controller.
extern NSString* const kUnitConversionTableViewIdentifier;
// Accessibility identifier for the source unit label.
extern NSString* const kSourceUnitLabelIdentifier;
// Accessibility identifier for the source unit menu button.
extern NSString* const kSourceUnitMenuButtonIdentifier;
// Accessibility identifier for the target unit label.
extern NSString* const kTargetUnitLabelIdentifier;
// Accessibility identifier for the target unit menu button.
extern NSString* const kTargetUnitMenuButtonIdentifier;
// Accessibility identifier for the source unit field.
extern NSString* const kSourceUnitFieldIdentifier;
// Accessibility identifier for the target unit field.
extern NSString* const kTargetUnitFieldIdentifier;

// UMA histogram names.
extern const char kSourceUnitChangeAfterUnitTypeChangeHistogram[];
extern const char kSourceUnitChangeBeforeUnitTypeChangeHistogram[];
extern const char kTargetUnitChangeHistogram[];

// An enum representing the different unit conversion types and measurement
// systems, it's persisted in logs, bucket should not be renumbered or reused.
// LINT.IfChange
enum class UnitConversionActionTypes {
  kUnchanged = 0,
  kAreaImperial = 1,
  kAreaMetric = 2,
  kInformationStorageImperial = 3,
  kInformationStorageMetric = 4,
  kLengthImperial = 5,
  kLengthMetric = 6,
  kMassImperial = 7,
  kMassMetric = 8,
  kSpeedImperial = 9,
  kSpeedMetric = 10,
  kTemperatureImperial = 11,
  kTemperatureMetric = 12,
  kVolumeImperial = 13,
  kVolumeMetric = 14,
  kMaxValue = kVolumeMetric,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml)

#endif  // IOS_CHROME_BROWSER_UNIT_CONVERSION_UI_BUNDLED_UNIT_CONVERSION_CONSTANTS_H_
