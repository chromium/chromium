// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_H_

namespace content {
struct MainFunctionParams;
}  // namespace content

int WebEngineBrowserMain(content::MainFunctionParams parameters);

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_H_
