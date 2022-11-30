// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_H_

#include "headless/lib/browser/headless_browser_impl.h"

namespace headless {

// Starts a DevTools HTTP handler on the loopback interface on the port
// configured by HeadlessBrowser::Options.
void StartLocalDevToolsHttpHandler(HeadlessBrowserImpl* browser);
void StopLocalDevToolsHttpHandler();

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_H_
