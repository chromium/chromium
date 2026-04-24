// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODEL_OPTION_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODEL_OPTION_H_

#include "base/containers/enum_set.h"

// The available model options.
enum class ComposeboxModelOption {
  // No explicit selection.
  kNone = 0,
  // Regular model in use.
  kRegular = 1,
  // The system automatically selects the optimal model per query.
  kAuto = 2,
  // The system utilizes the thinking reasoning engine.
  kThinking = 3,
  // The system utilizes the thinking reasoning engine with no gen UI mode.
  kThinkingNoGenUI = 4,
  // The maximum value for iteration.
  kMaxValue = kThinkingNoGenUI,
};

// A set of ComposeboxModelOption values used for iteration.
using ComposeboxModelOptionSet =
    base::EnumSet<ComposeboxModelOption,
                  ComposeboxModelOption::kNone,
                  ComposeboxModelOption::kMaxValue>;

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODEL_OPTION_H_
