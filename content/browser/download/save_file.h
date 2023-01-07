// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "components/download/public/common/base_file.h"
#include "content/browser/download/save_types.h"

namespace content {
// SaveFile ----------------------------------------------------------------

// These objects live exclusively on the download task runner and handle the
// writing operations for one save item. These objects live only for the
// duration that the saving job is 'in progress': once the saving job has been
// completed or canceled, the SaveFile is destroyed. One SaveFile object
// represents one item in a save session.
class SaveFile {
 public:
  SaveFile(std::unique_ptr<SaveFileCreateInfo> info, bool calculate_hash);

  SaveFile(const SaveFile&) = delete;
  SaveFile& operator=(const SaveFile&) = delete;

  virtual ~SaveFile();

  // BaseFile delegated functions.
  download::DownloadInterruptReason Initialize();
  download::DownloadInterruptReason AppendDataToFile(const char* data,
                                                     size_t data_len);
  download::DownloadInterruptReason Rename(const base::FilePath& full_path);
  void Detach();
  void Cancel();
  void Finish();
  void AnnotateWithSourceInformation(
      const std::string& client_guid,
      const GURL& source_url,
      const GURL& referrer_url,
      mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
      download::BaseFile::OnAnnotationDoneCallback on_annotation_done_callback);
  base::FilePath FullPath() const;
  bool InProgress() const;
  int64_t BytesSoFar() const;
  std::string DebugString() const;

  // Accessors.
  SaveItemId save_item_id() const { return info_->save_item_id; }
  int render_process_id() const { return info_->render_process_id; }
  SaveFileCreateInfo::SaveFileSource save_source() const {
    return info_->save_source;
  }
  const SaveFileCreateInfo& create_info() const { return *info_; }

 private:
  download::BaseFile file_;
  std::unique_ptr<SaveFileCreateInfo> info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_H_
