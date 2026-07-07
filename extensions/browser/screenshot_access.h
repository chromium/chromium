// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCREENSHOT_ACCESS_H_
#define EXTENSIONS_BROWSER_SCREENSHOT_ACCESS_H_

namespace extensions {

enum class ScreenshotAccessError {
  kDisabledByPreferences,
  kDisabledByDlp,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCREENSHOT_ACCESS_H_
