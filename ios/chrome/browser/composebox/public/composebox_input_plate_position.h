// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_PLATE_POSITION_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_PLATE_POSITION_H_

// The possible positions of the compose plate.
enum class ComposeboxInputPlatePosition {
  // Input plate on top.
  kTop = 0,
  // Input plate on bottom.
  kBottom = 1,
  // The input plate is currently unavailable (either deallocated or not yet
  // added).
  kMissing = 2
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_INPUT_PLATE_POSITION_H_
