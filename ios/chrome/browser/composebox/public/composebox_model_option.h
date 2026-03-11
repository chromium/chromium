// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODEL_OPTION_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODEL_OPTION_H_

// The available model options.
enum class ComposeboxModelOption {
  // No explicit selection.
  kNone,
  // Regular model in use.
  kRegular,
  // The system automatically selects the optimal model per query.
  kAuto,
  // The system utilizes the thinking reasoning engine.
  kThinking,
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODEL_OPTION_H_
