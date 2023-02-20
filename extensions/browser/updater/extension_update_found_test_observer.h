// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_UPDATE_FOUND_TEST_OBSERVER_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_UPDATE_FOUND_TEST_OBSERVER_H_

#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "extensions/browser/updater/extension_downloader.h"

namespace extensions {

// Useful for test code that wants to wait until an ExtensionDownloader has
// found an update for an extension.
//
// Usage example:
//  ExtensionUpdateFoundTestObserver update_found_observer;
//  [...] // test code that triggers creation of an ExtensionDownloader etc
//  update_found_observer.Wait();
class ExtensionUpdateFoundTestObserver
    : public ExtensionDownloader::TestObserver {
 public:
  ExtensionUpdateFoundTestObserver();
  ~ExtensionUpdateFoundTestObserver() override;
  void OnExtensionUpdateFound(const ExtensionId& id,
                              const std::set<int>& request_ids,
                              const base::Version& version) override;

  void Wait();

 private:
  base::RunLoop run_loop_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_UPDATE_FOUND_TEST_OBSERVER_H_
