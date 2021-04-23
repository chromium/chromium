// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_H_
#define CONTENT_BROWSER_BROWSER_MAIN_H_

#include "content/common/content_export.h"

namespace content {

struct MainFunctionParams;

CONTENT_EXPORT int BrowserMain(const content::MainFunctionParams& parameters);

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_MAIN_H_
