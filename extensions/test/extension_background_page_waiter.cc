// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/extension_background_page_waiter.h"

#include "base/scoped_observation.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

ExtensionBackgroundPageWaiter::ExtensionBackgroundPageWaiter(
    content::BrowserContext* browser_context,
    const Extension& extension)
    : browser_context_(browser_context),
      extension_(base::WrapRefCounted(&extension)) {}
ExtensionBackgroundPageWaiter::~ExtensionBackgroundPageWaiter() = default;

void ExtensionBackgroundPageWaiter::Wait() {
  if (browser_context_->IsOffTheRecord() &&
      !IncognitoInfo::IsSplitMode(extension_.get())) {
    ADD_FAILURE() << "Trying to wait for an incognito background page from a "
                  << "spanning mode extension. Use the on-the-record context.";
  }

  if (!BackgroundInfo::HasBackgroundPage(extension_.get()))
    return;  // No background page to wait for!

  if (extension_->is_hosted_app()) {
    // Little known fact: hosted apps can have background pages. They are
    // handled separately in BackgroundContents[Service], and don't use the
    // same infrastructure. They're also deprecated. Don't worry about them.
    // (If we see flakiness in loading hosted apps, we could potentially
    // rejigger this to accommodate for them as well, but it's unclear if it's
    // a problem that needs solving.)
    return;
  }

  ProcessManager* process_manager = ProcessManager::Get(browser_context_);
  ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForExtension(extension_->id());

  if (!extension_host) {
    // If the extension has a lazy background page, it's possible that it's
    // already been loaded and unloaded. As a proxy for this, check if there
    // are registered events.
    // This isn't a perfect solution, because
    // a) We might be waiting on a subsequent background page load, and
    // b) The extension might not register any events (which would normally be
    //    a bug in event page-based extensions, but not always).
    // But, it's a decent proxy for now.
    if (BackgroundInfo::HasLazyBackgroundPage(extension_.get()) &&
        EventRouter::Get(browser_context_)
            ->HasRegisteredEvents(extension_->id())) {
      return;
    }

    WaitForExtensionHostCreation();
    extension_host =
        process_manager->GetBackgroundHostForExtension(extension_->id());
  }

  ASSERT_TRUE(extension_host);
  if (extension_host->has_loaded_once()) {
    // The background host exists and has loaded; we're done.
    return;
  }

  WaitForExtensionHostReady(extension_host);
}

void ExtensionBackgroundPageWaiter::WaitForExtensionHostCreation() {
  process_manager_observation_.Observe(ProcessManager::Get(browser_context_));
  host_created_run_loop_.Run();
}

void ExtensionBackgroundPageWaiter::WaitForExtensionHostReady(
    ExtensionHost* host) {
  extension_host_observation_.Observe(host);
  host_ready_run_loop_.Run();
}

void ExtensionBackgroundPageWaiter::OnBackgroundHostCreated(
    ExtensionHost* host) {
  if (host->extension_id() != extension_->id() ||
      host->browser_context() != browser_context_) {
    return;
  }

  process_manager_observation_.Reset();
  host_created_run_loop_.QuitWhenIdle();
}

void ExtensionBackgroundPageWaiter::OnExtensionHostDidStopFirstLoad(
    const ExtensionHost* host) {
  ASSERT_EQ(extension_->id(), host->extension_id());
  ASSERT_TRUE(host->has_loaded_once());
  extension_host_observation_.Reset();
  host_ready_run_loop_.QuitWhenIdle();
}

void ExtensionBackgroundPageWaiter::OnExtensionHostDestroyed(
    ExtensionHost* host) {
  // This is only called while we're waiting for the host to be ready (since
  // we remove ourselves as an observer when it's done).
  DCHECK(host_ready_run_loop_.running());
  ASSERT_EQ(extension_->id(), host->extension_id());
  ADD_FAILURE() << "Extension host for " << extension_->name()
                << "was destroyed before it finished loading.";
  ASSERT_TRUE(extension_host_observation_.IsObservingSource(host));
  extension_host_observation_.Reset();
}

}  // namespace extensions
