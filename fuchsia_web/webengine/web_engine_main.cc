// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "components/fuchsia_component_support/config_reader.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "fuchsia_web/webengine/browser/web_engine_config.h"
#include "fuchsia_web/webengine/context_provider_main.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webengine/web_engine_main_delegate.h"

static void LoadConfigAndUpdateCommandLine(base::CommandLine* command_line) {
  // Config file needs to be loaded only in the browser process.
  bool is_browser_process =
      command_line->GetSwitchValueASCII(switches::kProcessType).empty();
  if (!is_browser_process)
    return;

  const absl::optional<base::Value::Dict>& config =
      fuchsia_component_support::LoadPackageConfig();
  if (!config)
    return;

  bool config_valid =
      UpdateCommandLineFromConfigFile(config.value(), command_line);
  if (!config_valid)
    LOG(FATAL) << "WebEngine config is invalid.";
}

int main(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);

  auto* const command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kContextProvider))
    return ContextProviderMain();

  LoadConfigAndUpdateCommandLine(command_line);

  WebEngineMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  // Repeated base::CommandLine::Init() is ignored, so it's safe to pass null
  // args here.
  params.argc = 0;
  params.argv = nullptr;

  return content::ContentMain(std::move(params));
}
