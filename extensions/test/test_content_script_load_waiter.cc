// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_content_script_load_waiter.h"

#include "content/public/browser/browser_context.h"

namespace extensions {

ContentScriptLoadWaiter::ContentScriptLoadWaiter(UserScriptLoader* loader)
    : host_id_(loader->host_id()) {
  loader_observation_.Observe(loader);
}
ContentScriptLoadWaiter::~ContentScriptLoadWaiter() = default;

void ContentScriptLoadWaiter::Wait() {
  run_loop_.Run();
}

void ContentScriptLoadWaiter::OnScriptsLoaded(
    UserScriptLoader* loader,
    content::BrowserContext* browser_context) {
  // Quit when idle in order to allow other observers to run.
  if (loader->HasLoadedScripts())
    run_loop_.QuitWhenIdle();
}

void ContentScriptLoadWaiter::OnUserScriptLoaderDestroyed(
    UserScriptLoader* loader) {
  loader_observation_.Reset();
}

}  // namespace extensions
