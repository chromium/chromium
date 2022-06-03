// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/app/content_main.h"
#include "fuchsia/engine/context_provider_main.h"
#include "fuchsia/engine/switches.h"
#include "fuchsia/engine/web_engine_main_delegate.h"

int main(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kContextProvider)) {
    return ContextProviderMain();
  }

  WebEngineMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  // Repeated base::CommandLine::Init() is ignored, so it's safe to pass null
  // args here.
  params.argc = 0;
  params.argv = nullptr;

  return content::ContentMain(std::move(params));
}
