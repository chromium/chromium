// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_update_found_test_observer.h"

namespace extensions {

ExtensionUpdateFoundTestObserver::ExtensionUpdateFoundTestObserver() {
  DCHECK(!ExtensionDownloader::test_observer());
  ExtensionDownloader::set_test_observer(this);
}

ExtensionUpdateFoundTestObserver::~ExtensionUpdateFoundTestObserver() {
  DCHECK(ExtensionDownloader::test_observer() == this);
  ExtensionDownloader::set_test_observer(nullptr);
}

void ExtensionUpdateFoundTestObserver::Wait() {
  run_loop_.Run();
}

void ExtensionUpdateFoundTestObserver::OnExtensionUpdateFound(
    const ExtensionId& id,
    const std::set<int>& request_ids,
    const base::Version& version) {
  run_loop_.Quit();
}

}  // namespace extensions
