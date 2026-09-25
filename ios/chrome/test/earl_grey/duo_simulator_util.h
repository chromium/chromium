// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_DUO_SIMULATOR_UTIL_H_
#define IOS_CHROME_TEST_EARL_GREY_DUO_SIMULATOR_UTIL_H_

#import <UIKit/UIKit.h>

// Returns true if running on an iPhone Duo simulator.
bool IsDuoSimulator();

// Sends a simulated hinge angle event (0.0 = Closed, 130.0 = Book,
// 180.0 = Open).
bool DispatchSimulatedDuoHingeAngle(double angle_in_degrees);

// Returns true if the window and hinge status (`Closed`, `PartiallyOpen`, or
// `FullyOpen`) match the posture expected for `angle_in_degrees`. Note that a
// true return value does not mean the hinge angle equals `angle_in_degrees`,
// only that the hinge status matches the bucket for that angle.
bool IsSimulatedDuoHingePostureSettled(double angle_in_degrees);

// Sends a simulated device orientation event.
bool DispatchSimulatedDuoOrientation(UIDeviceOrientation orientation);

// Returns true if the scene and window orientation match `orientation`.
bool IsSimulatedDuoOrientationSettled(UIDeviceOrientation orientation);

#endif  // IOS_CHROME_TEST_EARL_GREY_DUO_SIMULATOR_UTIL_H_
