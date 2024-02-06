// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_INTEREST_SIGNALS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_INTEREST_SIGNALS_H_

namespace default_browser {

// Records all necessary information for Chrome start with widget.
void NotifyStartWithWidget();

// Records all necessary information for Chrome start with URL event.
void NotifyStartWithURL();

}  // namespace default_browser

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_INTEREST_SIGNALS_H_
