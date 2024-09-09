// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_file.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_task_runner.h"

namespace content {

// TODO(asanka): SaveFile should use the target directory of the save package as
//               the default download directory when initializing |file_|.
//               Unfortunately, as it is, constructors of SaveFile don't always
//               have access to the SavePackage at this point.
SaveFile::SaveFile(std::unique_ptr<SaveFileCreateInfo> info,
                   bool calculate_hash)
    : file_(download::DownloadItem::kInvalidId), info_(std::move(info)) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());

  DCHECK(info_);
  DCHECK(info_->path.empty());
}

SaveFile::~SaveFile() {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
}

download::DownloadInterruptReason SaveFile::Initialize() {
  int64_t bytes_wasted = 0;
  download::DownloadInterruptReason reason = file_.Initialize(
      /*full_path=*/base::FilePath(), /*default_directory=*/base::FilePath(),
      /*file=*/base::File(), /*bytes_so_far=*/0, /*hash_so_far=*/std::string(),
      /*hash_state=*/nullptr, /*is_sparse_file=*/false,
      /*bytes_wasted*/ &bytes_wasted);
  info_->path = FullPath();
  return reason;
}

download::DownloadInterruptReason SaveFile::AppendDataToFile(const char* data,
                                                             size_t data_len) {
  return file_.AppendDataToFile(data, data_len);
}

download::DownloadInterruptReason SaveFile::Rename(
    const base::FilePath& full_path) {
  return file_.Rename(full_path);
}

void SaveFile::Detach() {
  file_.Detach();
}

void SaveFile::Cancel() {
  file_.Cancel();
}

void SaveFile::Finish() {
  file_.Finish();
}

void SaveFile::AnnotateWithSourceInformation(
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
    download::BaseFile::OnAnnotationDoneCallback on_annotation_done_callback) {
  // TODO(crbug.com/351165321): Consider propagating request_initiator
  // information here.
  file_.AnnotateWithSourceInformation(
      client_guid, source_url, referrer_url, /*request_initiator=*/std::nullopt,
      std::move(remote_quarantine), std::move(on_annotation_done_callback));
}

base::FilePath SaveFile::FullPath() const {
  return file_.full_path();
}

bool SaveFile::InProgress() const {
  return file_.in_progress();
}

int64_t SaveFile::BytesSoFar() const {
  return file_.bytes_so_far();
}

std::string SaveFile::DebugString() const {
  return file_.DebugString();
}

}  // namespace content
