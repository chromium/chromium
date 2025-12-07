// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRY_TEST_HELPER_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRY_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension.h"

namespace extensions {

// Class to observe service worker readiness for the execution of test JS.
class ExtensionRegistryTestHelper : public ExtensionRegistryObserver {
 public:
  explicit ExtensionRegistryTestHelper(const char* extension_id,
                                       content::BrowserContext* context);

  ~ExtensionRegistryTestHelper() override;

  std::optional<int> WaitForManifestVersion();

  void WaitForServiceWorkerStart();

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* context,
                         const Extension* extension) override;

 private:
  const std::string extension_id_;
  // This class is typically stack-allocated or as part of the test fixture,
  // making this safe. Tests must ensure that the browser context outlives this
  // helper.
  const raw_ptr<content::BrowserContext> context_;
  std::optional<int> manifest_version_;
  base::OnceClosure manifest_quit_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      scoped_observation_{this};
  service_worker_test_utils::TestServiceWorkerTaskQueueObserver
      started_observer_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRY_TEST_HELPER_H_
