// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_background_page_first_load_observer.h"

#include "base/check.h"

namespace extensions {

TestBackgroundPageFirstLoadObserver::TestBackgroundPageFirstLoadObserver(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id)
    : extension_id_(extension_id),
      process_manager_(ProcessManager::Get(browser_context)) {
  process_manager_observer_.Add(process_manager_);
  extension_host_ =
      process_manager_->GetBackgroundHostForExtension(extension_id_);
  if (extension_host_)
    OnObtainedExtensionHost();
}

TestBackgroundPageFirstLoadObserver::~TestBackgroundPageFirstLoadObserver() =
    default;

void TestBackgroundPageFirstLoadObserver::Wait() {
  if (!extension_host_ || !extension_host_->has_loaded_once())
    run_loop_.Run();
}

void TestBackgroundPageFirstLoadObserver::OnBackgroundHostCreated(
    ExtensionHost* host) {
  if (host->extension_id() == extension_id_) {
    DCHECK(!extension_host_);
    extension_host_ = host;
    OnObtainedExtensionHost();
  }
}

void TestBackgroundPageFirstLoadObserver::OnExtensionHostDestroyed(
    ExtensionHost* host) {
  if (host->extension_id() == extension_id_) {
    extension_host_observer_.Remove(host);
    extension_host_ = nullptr;
  }
}

void TestBackgroundPageFirstLoadObserver::OnExtensionHostDidStopFirstLoad(
    const ExtensionHost* host) {
  run_loop_.Quit();
}

void TestBackgroundPageFirstLoadObserver::OnObtainedExtensionHost() {
  extension_host_observer_.Add(extension_host_);
}

}  // namespace extensions
