// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TASK_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TASK_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/version.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "url/gurl.h"

namespace extensions {

// A struct which wraps parameters for a single extension update request.
struct ExtensionDownloaderTask {
  ExtensionDownloaderTask(const ExtensionDownloaderTask&) = delete;
  ExtensionDownloaderTask& operator=(const ExtensionDownloaderTask&) = delete;

  ExtensionDownloaderTask(ExtensionId id,
                          GURL update_url,
                          mojom::ManifestLocation install_location,
                          bool is_corrupt_reinstall,
                          int request_id,
                          DownloadFetchPriority fetch_priority,
                          base::Version version,
                          Manifest::Type type,
                          std::string update_url_data);
  ExtensionDownloaderTask(ExtensionId id,
                          GURL update_url,
                          mojom::ManifestLocation install_location,
                          bool is_corrupt_reinstall,
                          int request_id,
                          DownloadFetchPriority fetch_priority);

  ExtensionDownloaderTask(ExtensionDownloaderTask&&);
  ExtensionDownloaderTask& operator=(ExtensionDownloaderTask&&);

  ~ExtensionDownloaderTask();

  ExtensionId id;
  GURL update_url;
  mojom::ManifestLocation install_location;

  // Indicates that we detected corruption in the local copy of the extension
  // and we want to perform a reinstall of it.
  bool is_corrupt_reinstall{false};

  // Passed on as is to the various `delegate_` callbacks. This is used for
  // example by clients (e.g. ExtensionUpdater) to keep track of when
  // potentially concurrent update checks complete.
  int request_id;

  // Notifies the downloader the priority of this extension update (either
  // foreground or background).
  DownloadFetchPriority fetch_priority{DownloadFetchPriority::kBackground};

  // Specifies the version of the already downloaded crx file, equals
  // to 0.0.0.0 if there is no crx file or for a pending extension so it will
  // always be updated, and thus installed (assuming all extensions have
  // non-zero versions).
  base::Version version{"0.0.0.0"};

  // Used for metrics only and can be TYPE_UNKNOWN if e.g. the extension is
  // not yet installed.
  Manifest::Type type{Manifest::TYPE_UNKNOWN};

  // May be used to pass some additional data to the update server.
  std::string update_url_data;

  // Link to the delegate, set by ExtensionDownloader.
  raw_ptr<ExtensionDownloaderDelegate, DanglingUntriaged> delegate{nullptr};

  // Notifies delegate about stage change.
  void OnStageChanged(ExtensionDownloaderDelegate::Stage stage);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TASK_H_
