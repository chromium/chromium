// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODE_H_

#include "base/containers/enum_set.h"

// The different modes for the composebox.
enum class ComposeboxMode {
  // Performs a regular search.
  kRegularSearch = 0,
  // Performs an AI Mode search.
  kAIM = 1,
  // Creates an image based on the input.
  kImageGeneration = 2,
  // Generates a new canvas based on the input query.
  kCanvas = 3,
  // Helps user with complex research tasks.
  kDeepSearch = 4,
  // The maximum value for iteration.
  kMaxValue = kDeepSearch,
};

// A set of ComposeboxMode values used for iteration.
using ComposeboxModeSet = base::EnumSet<ComposeboxMode,
                                        ComposeboxMode::kRegularSearch,
                                        ComposeboxMode::kMaxValue>;

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODE_H_
