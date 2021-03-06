// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_BACKGROUND_PAGE_FIRST_LOAD_OBSERVER_H_
#define EXTENSIONS_TEST_TEST_BACKGROUND_PAGE_FIRST_LOAD_OBSERVER_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Allows to wait until the WebContents of an extension's ExtensionHost sees its
// first DidStopLoading().
class TestBackgroundPageFirstLoadObserver : public ProcessManagerObserver,
                                            public ExtensionHostObserver {
 public:
  TestBackgroundPageFirstLoadObserver(content::BrowserContext* browser_context,
                                      const ExtensionId& extension_id);
  ~TestBackgroundPageFirstLoadObserver() override;

  void Wait();

 private:
  // ProcessManagerObserver:
  void OnBackgroundHostCreated(ExtensionHost* host) override;

  // ExtensionHostObserver:
  void OnExtensionHostDestroyed(ExtensionHost* host) override;
  void OnExtensionHostDidStopFirstLoad(const ExtensionHost* host) override;

  void OnObtainedExtensionHost();

  const ExtensionId extension_id_;
  ProcessManager* const process_manager_ = nullptr;
  ExtensionHost* extension_host_ = nullptr;
  base::RunLoop run_loop_;
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_{this};
  ScopedObserver<ExtensionHost, ExtensionHostObserver> extension_host_observer_{
      this};

  DISALLOW_COPY_AND_ASSIGN(TestBackgroundPageFirstLoadObserver);
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_BACKGROUND_PAGE_FIRST_LOAD_OBSERVER_H_
