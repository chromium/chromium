// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_file_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "content/browser/media/cdm_storage_manager.h"
#include "content/browser/media/media_license_storage_host.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/file_system/file_system_types.h"

namespace content {

namespace {

// The CDM interface has a restriction that file names can not begin with _,
// so use it to prefix temporary files.
const char kTemporaryFilePrefix = '_';

// Maximum length of a file name.
const size_t kFileNameMaxLength = 256;

// UMA suffices for CDM File IO operations.
const char kReadFile[] = "ReadFile";
const char kWriteFile[] = "WriteFile";
const char kDeleteFile[] = "DeleteFile";

}  // namespace

// static
bool CdmFileImpl::IsValidName(const std::string& name) {
  // File names must only contain letters (A-Za-z), digits(0-9), or "._-",
  // and not start with "_". It must contain at least 1 character, and not
  // more then |kFileNameMaxLength| characters.
  if (name.empty() || name.length() > kFileNameMaxLength ||
      name[0] == kTemporaryFilePrefix) {
    return false;
  }

  for (const auto ch : name) {
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch) && ch != '.' &&
        ch != '_' && ch != '-') {
      return false;
    }
  }

  return true;
}

CdmFileImpl::CdmFileImpl(
    MediaLicenseStorageHost* host,
    const media::CdmType& cdm_type,
    const std::string& file_name,
    mojo::PendingAssociatedReceiver<media::mojom::CdmFile> pending_receiver)
    : file_name_(file_name), cdm_type_(cdm_type), host_(host) {
  DVLOG(3) << __func__ << " " << file_name_;
  DCHECK(IsValidName(file_name_));
  DCHECK(host_);

  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &CdmFileImpl::OnReceiverDisconnect, weak_factory_.GetWeakPtr()));
}

CdmFileImpl::CdmFileImpl(
    CdmStorageManager* manager,
    const blink::StorageKey& storage_key,
    const media::CdmType& cdm_type,
    const std::string& file_name,
    mojo::PendingAssociatedReceiver<media::mojom::CdmFile> pending_receiver)
    : file_name_(file_name),
      cdm_type_(cdm_type),
      storage_key_(storage_key),
      cdm_storage_manager_(manager) {
  DVLOG(3) << __func__ << " " << file_name_;
  DCHECK(IsValidName(file_name_));
  DCHECK(cdm_storage_manager_);

  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &CdmFileImpl::OnReceiverDisconnect, weak_factory_.GetWeakPtr()));
}

CdmFileImpl::~CdmFileImpl() {
  DVLOG(3) << __func__ << " " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (read_callback_)
    std::move(read_callback_).Run(Status::kFailure, {});

  if (write_callback_)
    std::move(write_callback_).Run(Status::kFailure);
}

void CdmFileImpl::Read(ReadCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(host_ || cdm_storage_manager_);

  // Only 1 Read() or Write() is allowed at any time.
  if (read_callback_ || write_callback_) {
    std::move(callback).Run(Status::kFailure, {});
    return;
  }

  // Save |callback| for later use.
  read_callback_ = std::move(callback);
  start_time_ = base::TimeTicks::Now();

  if (host_) {
    host_->ReadFile(
        cdm_type_, file_name_,
        base::BindOnce(&CdmFileImpl::DidRead, weak_factory_.GetWeakPtr()));
  } else {
    cdm_storage_manager_->ReadFile(
        storage_key_, cdm_type_, file_name_,
        base::BindOnce(&CdmFileImpl::DidRead, weak_factory_.GetWeakPtr()));
  }
}

void CdmFileImpl::DidRead(std::optional<std::vector<uint8_t>> data) {
  DVLOG(3) << __func__ << " file: " << file_name_
           << ", success: " << (data.has_value() ? "yes" : "no");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(read_callback_);
  DCHECK(host_ || cdm_storage_manager_);

  bool success = data.has_value();
  ReportFileOperationUMA(success, kReadFile);

  if (!success) {
    // Unable to read the contents of the file.
    std::move(read_callback_).Run(Status::kFailure, {});
    return;
  }

  std::move(read_callback_).Run(Status::kSuccess, std::move(data.value()));
}

void CdmFileImpl::Write(const std::vector<uint8_t>& data,
                        WriteCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name_ << ", size: " << data.size();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(host_ || cdm_storage_manager_);

  // Only 1 Read() or Write() is allowed at any time.
  if (read_callback_ || write_callback_) {
    std::move(callback).Run(Status::kFailure);
    return;
  }

  // Files are limited in size, so fail if file too big. This should have been
  // checked by the caller, but we don't fully trust IPC.
  if (data.size() > media::kMaxFileSizeBytes) {
    DLOG(WARNING) << __func__
                  << " Too much data to write. #bytes = " << data.size();
    std::move(callback).Run(Status::kFailure);
    return;
  }

  // Save |callback| for later use.
  write_callback_ = std::move(callback);
  start_time_ = base::TimeTicks::Now();

  // If there is no data to write, delete the file to save space.
  // |write_callback_| will be called after the file is deleted.
  if (data.empty()) {
    DeleteFile();
    return;
  }

  if (host_) {
    host_->WriteFile(
        cdm_type_, file_name_, data,
        base::BindOnce(&CdmFileImpl::DidWrite, weak_factory_.GetWeakPtr()));
  } else {
    cdm_storage_manager_->WriteFile(
        storage_key_, cdm_type_, file_name_, data,
        base::BindOnce(&CdmFileImpl::DidWrite, weak_factory_.GetWeakPtr()));
  }
}

void CdmFileImpl::ReportFileOperationUMA(bool success,
                                         const std::string& operation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(host_ || cdm_storage_manager_);

  // Strings for UMA names.
  static const char kUmaPrefix[] = "Media.EME.CdmFileIO";
  static const char kTimeTo[] = "TimeTo";

  const bool in_memory =
      (host_) ? host_->in_memory() : cdm_storage_manager_->in_memory();
  const std::string mode_suffix = in_memory ? "Incognito" : "Normal";

  // Records the result to the base histogram as well as splitting it out by
  // incognito or normal mode.
  auto result_uma_name = base::JoinString({kUmaPrefix, operation}, ".");
  base::UmaHistogramBoolean(result_uma_name, success);
  base::UmaHistogramBoolean(
      base::JoinString({result_uma_name, mode_suffix}, "."), success);

  // Records the time taken to the base histogram as well as splitting it out by
  // incognito or normal mode. Only reported for successful operation.
  if (success) {
    auto time_taken = base::TimeTicks::Now() - start_time_;
    auto time_taken_uma_name =
        base::JoinString({kUmaPrefix, kTimeTo, operation}, ".");
    base::UmaHistogramTimes(time_taken_uma_name, time_taken);
    base::UmaHistogramTimes(
        base::JoinString({time_taken_uma_name, mode_suffix}, "."), time_taken);
  }
}

void CdmFileImpl::DidWrite(bool success) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(write_callback_);
  DCHECK(host_ || cdm_storage_manager_);

  ReportFileOperationUMA(success, kWriteFile);

  if (!success) {
    DLOG(WARNING) << "Unable to write to file " << file_name_;
    std::move(write_callback_).Run(Status::kFailure);
    return;
  }

  std::move(write_callback_).Run(Status::kSuccess);
}

void CdmFileImpl::DeleteFile() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(write_callback_);
  DCHECK(host_ || cdm_storage_manager_);

  DVLOG(3) << "Deleting " << file_name_;

  if (host_) {
    host_->DeleteFile(cdm_type_, file_name_,
                      base::BindOnce(&CdmFileImpl::DidDeleteFile,
                                     weak_factory_.GetWeakPtr()));
  } else {
    cdm_storage_manager_->DeleteFile(
        storage_key_, cdm_type_, file_name_,
        base::BindOnce(&CdmFileImpl::DidDeleteFile,
                       weak_factory_.GetWeakPtr()));
  }
}

void CdmFileImpl::DidDeleteFile(bool success) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(write_callback_);
  DCHECK(host_ || cdm_storage_manager_);

  ReportFileOperationUMA(success, kDeleteFile);

  if (!success) {
    DLOG(WARNING) << "Unable to delete file " << file_name_;
    std::move(write_callback_).Run(Status::kFailure);
    return;
  }

  std::move(write_callback_).Run(Status::kSuccess);
}

void CdmFileImpl::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(host_ || cdm_storage_manager_);

  // May delete `this`.
  if (host_) {
    host_->OnFileReceiverDisconnect(file_name_, cdm_type_,
                                    base::PassKey<CdmFileImpl>());
  } else {
    cdm_storage_manager_->OnFileReceiverDisconnect(
        file_name_, cdm_type_, storage_key_, base::PassKey<CdmFileImpl>());
  }
}

}  // namespace content
