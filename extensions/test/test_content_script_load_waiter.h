// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_CONTENT_SCRIPT_LOAD_WAITER_H_
#define EXTENSIONS_TEST_TEST_CONTENT_SCRIPT_LOAD_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "extensions/browser/user_script_loader.h"

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

  // Restricts the waiter to wait until scripts from the provided HostID are
  // loaded.
  void RestrictToHostID(const HostID& host_id);

  // Waits until the observed UserScriptLoader completes a script load via the
  // OnScriptsLoaded event.
  void Wait();

 private:
  // UserScriptLoader::Observer:
  void OnScriptsLoaded(UserScriptLoader* loader,
                       content::BrowserContext* browser_context) override;
  void OnUserScriptLoaderDestroyed(UserScriptLoader* loader) override;

  HostID host_id_;
  base::RunLoop run_loop_;
  ScopedObserver<UserScriptLoader, UserScriptLoader::Observer> scoped_observer_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_CONTENT_SCRIPT_LOAD_WAITER_H_
