// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_ENUM_ENUM_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_ENUM_ENUM_H_

// Typed enum.
enum Color { kRed = 0, kBlue, kGreen, kYellow = 10 };

// Untyped enum.
enum { kOne = 1, kTwo, kThree, kFour };

// enum class.
enum class Pet { goat = 5, cat = 15, dogcow, rabbit };

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_ENUM_ENUM_H_
