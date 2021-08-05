// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_DOWNLOAD_MANAGER_DELEGATE_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/download_manager_delegate.h"

namespace content {

class DownloadManager;

class ShellDownloadManagerDelegate : public DownloadManagerDelegate {
 public:
  ShellDownloadManagerDelegate();
  ~ShellDownloadManagerDelegate() override;

  void SetDownloadManager(DownloadManager* manager);

  void Shutdown() override;
  bool DetermineDownloadTarget(download::DownloadItem* download,
                               DownloadTargetCallback* callback) override;
  bool ShouldOpenDownload(download::DownloadItem* item,
                          DownloadOpenDelayedCallback callback) override;
  void GetNextId(DownloadIdCallback callback) override;

  // Inhibits prompting and sets the default download path.
  void SetDownloadBehaviorForTesting(
      const base::FilePath& default_download_path);

 private:
  friend class base::RefCountedThreadSafe<ShellDownloadManagerDelegate>;

  using FilenameDeterminedCallback =
      base::OnceCallback<void(const base::FilePath&)>;

  static void GenerateFilename(const GURL& url,
                               const std::string& content_disposition,
                               const std::string& suggested_filename,
                               const std::string& mime_type,
                               const base::FilePath& suggested_directory,
                               FilenameDeterminedCallback callback);
  void OnDownloadPathGenerated(uint32_t download_id,
                               DownloadTargetCallback callback,
                               const base::FilePath& suggested_path);
  void ChooseDownloadPath(uint32_t download_id,
                          DownloadTargetCallback callback,
                          const base::FilePath& suggested_path);

  DownloadManager* download_manager_;
  base::FilePath default_download_path_;
  bool suppress_prompting_;
  base::WeakPtrFactory<ShellDownloadManagerDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShellDownloadManagerDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_DOWNLOAD_MANAGER_DELEGATE_H_
