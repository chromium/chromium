// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOWNLOAD_MANAGER_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_manager_delegate.h"

namespace base {
class FilePath;
}

namespace content {

class DownloadManager;

namespace protocol {

class DevToolsDownloadManagerDelegate
    : public base::SupportsUserData::Data,
      public content::DownloadManagerDelegate {
 public:
  enum class DownloadBehavior {
    // All downloads are denied.
    DENY,

    // All downloads are accepted.
    ALLOW,

    // All downloads are accepted and named using Guids.
    ALLOW_AND_NAME,

    // Use default download behavior if available, otherwise deny.
    DEFAULT
  };

  // Takes over the |browser_Context|'s download manager.
  // When existing delegate is set, this proxy will use the original's
  // |GetNextId| function to ensure compatibility. It will also call its
  // |Shutdown| method when shutting down and it will fallback to the original
  // delegate if it cannot find any DevToolsDownloadManagerHelper associated
  // with the download.
  static DevToolsDownloadManagerDelegate* GetOrCreateInstance(
      content::BrowserContext* browser_Context);
  static DevToolsDownloadManagerDelegate* GetInstance(
      content::BrowserContext* browser_Context);

  DevToolsDownloadManagerDelegate(const DevToolsDownloadManagerDelegate&) =
      delete;
  DevToolsDownloadManagerDelegate& operator=(
      const DevToolsDownloadManagerDelegate&) = delete;

  ~DevToolsDownloadManagerDelegate() override = default;

  void set_download_behavior(DownloadBehavior behavior) {
    download_behavior_ = behavior;
  }
  void set_download_path(const std::string& path) { download_path_ = path; }

  // DownloadManagerDelegate overrides.
  void Shutdown() override;
  bool DetermineDownloadTarget(
      download::DownloadItem* download,
      download::DownloadTargetCallback* callback) override;
  bool ShouldOpenDownload(
      download::DownloadItem* item,
      content::DownloadOpenDelayedCallback callback) override;
  void GetNextId(content::DownloadIdCallback callback) override;
  download::DownloadItem* GetDownloadByGuid(const std::string& guid) override;

 private:
  friend class base::RefCounted<DevToolsDownloadManagerDelegate>;

  explicit DevToolsDownloadManagerDelegate(BrowserContext* browser_context);

  using FilenameDeterminedCallback =
      base::OnceCallback<void(const base::FilePath&)>;

  static void GenerateFilename(const GURL& url,
                               const std::string& content_disposition,
                               const std::string& suggested_filename,
                               const std::string& mime_type,
                               const base::FilePath& suggested_directory,
                               FilenameDeterminedCallback callback);

  void OnDownloadPathGenerated(uint32_t download_id,
                               download::DownloadTargetCallback callback,
                               const base::FilePath& suggested_path);

  raw_ptr<content::DownloadManager> download_manager_;
  raw_ptr<content::DownloadManagerDelegate> original_download_delegate_;
  DownloadBehavior download_behavior_ = DownloadBehavior::DEFAULT;
  std::string download_path_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVTOOLS_DOWNLOAD_MANAGER_DELEGATE_H_
