// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_browser_main.h"

#include <memory>

#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_runner.h"

int WebEngineBrowserMain(content::MainFunctionParams parameters) {
  std::unique_ptr<content::BrowserMainRunner> main_runner =
      content::BrowserMainRunner::Create();
  int exit_code = main_runner->Initialize(std::move(parameters));
  if (exit_code >= 0)
    return exit_code;

  exit_code = main_runner->Run();

  main_runner->Shutdown();

  return exit_code;
}
