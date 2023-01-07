// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_DELEGATE_H_

#include <vector>

#include "extensions/browser/updater/extension_downloader_task.h"

namespace extensions {

class ExtensionDownloader;
class ExtensionDownloaderDelegate;

// A class for intercepting the work of checking for / downloading extension
// updates.
class ExtensionDownloaderTestDelegate {
 public:
  // This method gets called when an update check is being started for an
  // extension. Normally implementors should eventually call either
  // OnExtensionDownloadFailed or OnExtensionDownloadFinished on
  // |delegate|.
  virtual void StartUpdateCheck(ExtensionDownloader* downloader,
                                ExtensionDownloaderDelegate* delegate,
                                std::vector<ExtensionDownloaderTask> tasks) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TEST_DELEGATE_H_
