// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_content_script_load_waiter.h"

#include "content/public/browser/browser_context.h"

namespace extensions {

ContentScriptLoadWaiter::ContentScriptLoadWaiter(UserScriptLoader* loader)
    : scoped_observer_(this) {
  scoped_observer_.Add(loader);
}
ContentScriptLoadWaiter::~ContentScriptLoadWaiter() = default;

void ContentScriptLoadWaiter::RestrictToHostID(const HostID& host_id) {
  host_id_ = host_id;
}

void ContentScriptLoadWaiter::Wait() {
  run_loop_.Run();
}

void ContentScriptLoadWaiter::OnScriptsLoaded(
    UserScriptLoader* loader,
    content::BrowserContext* browser_context) {
  if (host_id_.id().empty() || loader->HasLoadedScripts(host_id_)) {
    // Quit when idle in order to allow other observers to run.
    run_loop_.QuitWhenIdle();
  }
}
void ContentScriptLoadWaiter::OnUserScriptLoaderDestroyed(
    UserScriptLoader* loader) {}

}  // namespace extensions
