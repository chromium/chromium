// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_browser_main.h"

#include <memory>

#include "base/logging.h"
#include "build/build_config.h"
#include "components/fuchsia_component_support/config_reader.h"
#include "content/public/browser/browser_main_runner.h"
#include "fuchsia_web/webengine/browser/web_engine_config.h"

int WebEngineBrowserMain(content::MainFunctionParams parameters) {
  // Process package config-data if any.
  const absl::optional<base::Value>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (config) {
    bool config_valid = UpdateCommandLineFromConfigFile(
        config.value(), base::CommandLine::ForCurrentProcess());
    if (!config_valid)
      LOG(FATAL) << "WebEngine config is invalid.";
  }

  std::unique_ptr<content::BrowserMainRunner> main_runner =
      content::BrowserMainRunner::Create();
  int exit_code = main_runner->Initialize(std::move(parameters));
  if (exit_code >= 0)
    return exit_code;

  exit_code = main_runner->Run();

  main_runner->Shutdown();

  return exit_code;
}
