// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry_test_helper.h"

#include "base/run_loop.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"

namespace extensions {

ExtensionRegistryTestHelper::ExtensionRegistryTestHelper(
    const char* extension_id,
    content::BrowserContext* context)
    : extension_id_(extension_id), context_(context) {
  scoped_observation_.Observe(extensions::ExtensionRegistry::Get(context_));
}

ExtensionRegistryTestHelper::~ExtensionRegistryTestHelper() = default;

std::optional<int> ExtensionRegistryTestHelper::WaitForManifestVersion() {
  if (manifest_version_) {
    return manifest_version_;
  }
  base::RunLoop waiter;
  manifest_quit_ = waiter.QuitClosure();
  waiter.Run();
  return manifest_version_;
}

void ExtensionRegistryTestHelper::WaitForServiceWorkerStart() {
  // TestServiceWorkerTaskQueueObserver is designed such that all three events
  // must be waited for, in order, as any event will trigger the end of the
  // waiter. In order to know whether a worker started, we must be sure we
  // received an "OnActiveExtension" event and a "WorkerContextInitialized"
  // event first. Failures will also trigger any of these waiters to return. If
  // a timeout occurs in a test waiting for this function, it might be because a
  // failure triggered the stop of a waiter and no subsequent events are
  // received.
  started_observer_.WaitForOnActivateExtension(extension_id_);
  started_observer_.WaitForWorkerContextInitialized(extension_id_);
  started_observer_.WaitForWorkerStarted(extension_id_);
}

void ExtensionRegistryTestHelper::OnExtensionLoaded(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  if (context == context_ && extension->id() == extension_id_) {
    manifest_version_ = extension->manifest_version();
    if (manifest_quit_) {
      std::move(manifest_quit_).Run();
    }
  }
}

}  // namespace extensions
