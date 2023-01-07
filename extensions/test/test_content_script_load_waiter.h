// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_CONTENT_SCRIPT_LOAD_WAITER_H_
#define EXTENSIONS_TEST_TEST_CONTENT_SCRIPT_LOAD_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/mojom/host_id.mojom.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A single-use class to wait for content scripts to be loaded.
class ContentScriptLoadWaiter : public UserScriptLoader::Observer {
 public:
  explicit ContentScriptLoadWaiter(UserScriptLoader* loader);
  ~ContentScriptLoadWaiter();
  ContentScriptLoadWaiter(const ContentScriptLoadWaiter& other) = delete;
  ContentScriptLoadWaiter& operator=(const ContentScriptLoadWaiter& other) =
      delete;

  // Waits until the observed UserScriptLoader completes a script load via the
  // OnScriptsLoaded event.
  void Wait();

 private:
  // UserScriptLoader::Observer:
  void OnScriptsLoaded(UserScriptLoader* loader,
                       content::BrowserContext* browser_context) override;
  void OnUserScriptLoaderDestroyed(UserScriptLoader* loader) override;

  mojom::HostID host_id_;

  base::RunLoop run_loop_;
  base::ScopedObservation<UserScriptLoader, UserScriptLoader::Observer>
      loader_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_CONTENT_SCRIPT_LOAD_WAITER_H_
