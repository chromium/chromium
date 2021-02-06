// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_EXTENSION_BACKGROUND_PAGE_WAITER_H_
#define EXTENSIONS_TEST_EXTENSION_BACKGROUND_PAGE_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A class to wait for an extension's background page to finish its first load.
// Note: This does not accommodate ServiceWorker-based extensions.
// TODO(devlin): Combine this and BackgroundPageWatcher? They are subtlely
// different (BackgroundPageWatcher waits for the page to be in a certain state,
// such as the background page being open at the time, whereas this waits for
// the page to *have been opened* at some point), but similar enough that we
// probably don't need two separate classes.
class ExtensionBackgroundPageWaiter : public ProcessManagerObserver,
                                      public ExtensionHostObserver {
 public:
  ExtensionBackgroundPageWaiter(content::BrowserContext* browser_context,
                                const Extension& extension);
  ExtensionBackgroundPageWaiter(const ExtensionBackgroundPageWaiter& other) =
      delete;
  ExtensionBackgroundPageWaiter& operator=(
      const ExtensionBackgroundPageWaiter& other) = delete;
  ~ExtensionBackgroundPageWaiter() override;

  void Wait();

 private:
  // Waits for the ExtensionHost to be created by the ProcessManager.
  void WaitForExtensionHostCreation();

  // Waits for the ExtensionHost to finish its first load cycle.
  void WaitForExtensionHostReady(ExtensionHost* host);

  // ProcessManagerObserver:
  void OnBackgroundHostCreated(ExtensionHost* host) override;

  // ExtensionHostObserver:
  void OnExtensionHostDidStopFirstLoad(const ExtensionHost* host) override;
  void OnExtensionHostDestroyed(ExtensionHost* host) override;

  content::BrowserContext* const browser_context_;
  scoped_refptr<const Extension> extension_;
  base::RunLoop host_ready_run_loop_;
  base::RunLoop host_created_run_loop_;
  ScopedObserver<ExtensionHost, ExtensionHostObserver> extension_host_observer_{
      this};
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_EXTENSION_BACKGROUND_PAGE_WAITER_H_
