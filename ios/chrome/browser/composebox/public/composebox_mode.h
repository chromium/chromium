// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODE_H_

// The different modes for the composebox.
enum class ComposeboxMode {
  // Performs a regular search.
  kRegularSearch,
  // Performs an AI Mode search.
  kAIM,
  // Creates an image based on the input.
  kImageGeneration,
  // Generates a new canvas based on the input query.
  kCanvas,
  // Helps user with complex research tasks.
  kDeepSearch,
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_MODE_H_
