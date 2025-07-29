// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/services/mojo_cdm_file_io.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/cdm/cdm_helpers.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace media {

namespace {

using ClientStatus = cdm::FileIOClient::Status;
using FileStatus = media::mojom::CdmFile::Status;
using StorageStatus = media::mojom::CdmStorage::Status;

// Constants for UMA reporting of file size (in KB) via
// UMA_HISTOGRAM_CUSTOM_COUNTS. Note that the histogram is log-scaled (rather
// than linear).
constexpr int kSizeKBMin = 1;
const int64_t kMaxFileSizeKB = 512;
constexpr int kSizeKBBuckets = 100;

const char* ConvertStorageStatus(StorageStatus status) {
  switch (status) {
    case StorageStatus::kSuccess:
      return "kSuccess";
    case StorageStatus::kInUse:
      return "kInUse";
    case StorageStatus::kFailure:
      return "kFailure";
  }

  return "unknown";
}

const char* ConvertFileStatus(FileStatus status) {
  return status == FileStatus::kSuccess ? "kSuccess" : "kFailure";
}

}  // namespace

MojoCdmFileIO::MojoCdmFileIO(Delegate* delegate,
                             cdm::FileIOClient* client,
                             mojo::Remote<mojom::CdmStorage> cdm_storage)
    : delegate_(delegate),
      client_(client),
      cdm_storage_(std::move(cdm_storage)) {
  DVLOG(1) << __func__;
  DCHECK(delegate_);
  DCHECK(client_);
  DCHECK(cdm_storage_);
}

MojoCdmFileIO::~MojoCdmFileIO() {
  DVLOG(1) << __func__;
}

void MojoCdmFileIO::Open(const char* file_name, uint32_t file_name_size) {
  std::string file_name_string(file_name, file_name_size);
  DVLOG(3) << __func__ << " file: " << file_name_string;

  // Open is only allowed if the current state is kUnopened and the file name
  // is valid.
  if (state_ != State::kUnopened) {
    OnError(ErrorType::kOpenError);
    return;
  }

  state_ = State::kOpening;
  file_name_ = file_name_string;

  TRACE_EVENT_ASYNC_BEGIN1("media", "MojoCdmFileIO::Open", this, "file_name",
                           file_name_);

  // Wrap the callback to detect the case when the mojo connection is
  // terminated prior to receiving the response. This avoids problems if the
  // service is destroyed before the CDM. If that happens let the CDM know that
  // Open() failed.
  auto callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&MojoCdmFileIO::OnFileOpened, weak_factory_.GetWeakPtr()),
      StorageStatus::kFailure, mojo::NullAssociatedRemote());
  cdm_storage_->Open(file_name_string, std::move(callback));
}

void MojoCdmFileIO::OnFileOpened(
    StorageStatus status,
    mojo::PendingAssociatedRemote<mojom::CdmFile> cdm_file) {
  DVLOG(3) << __func__ << " file: " << file_name_ << ", status: " << status;

  UMA_HISTOGRAM_ENUMERATION("Media.EME.CdmFileIO::OpenFile", status);

  // This logs the end of the async Open() request, and separately logs
  // how long the client takes in OnOpenComplete().
  TRACE_EVENT_ASYNC_END1("media", "MojoCdmFileIO::Open", this, "status",
                         ConvertStorageStatus(status));
  switch (status) {
    case StorageStatus::kSuccess:
      // File was successfully opened.
      state_ = State::kOpened;
      cdm_file_.Bind(std::move(cdm_file));
      {
        TRACE_EVENT0("media", "FileIOClient::OnOpenComplete");
        client_->OnOpenComplete(ClientStatus::kSuccess);
      }
      return;
    case StorageStatus::kInUse:
      // File already open by somebody else.
      state_ = State::kUnopened;
      OnError(ErrorType::kOpenInUse);
      return;
    case StorageStatus::kFailure:
      // Something went wrong.
      state_ = State::kError;
      OnError(ErrorType::kOpenError);
      return;
  }

  NOTREACHED();
}

void MojoCdmFileIO::Read() {
  DVLOG(3) << __func__ << " file: " << file_name_;

  // If another operation is in progress, fail.
  if (state_ == State::kReading || state_ == State::kWriting) {
    OnError(ErrorType::kReadInUse);
    return;
  }

  // If the file is not open, fail.
  if (state_ != State::kOpened) {
    OnError(ErrorType::kReadError);
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1("media", "MojoCdmFileIO::Read", this, "file_name",
                           file_name_);

  state_ = State::kReading;

  // Wrap the callback to detect the case when the mojo connection is
  // terminated prior to receiving the response. This avoids problems if the
  // service is destroyed before the CDM. If that happens let the CDM know that
  // Read() failed.
  auto callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&MojoCdmFileIO::OnFileRead, weak_factory_.GetWeakPtr()),
      FileStatus::kFailure, std::vector<uint8_t>());
  cdm_file_->Read(std::move(callback));
}

void MojoCdmFileIO::OnFileRead(FileStatus status,
                               const std::vector<uint8_t>& data) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_EQ(State::kReading, state_);

  // This logs the end of the async Read() request, and separately logs
  // how long the client takes in OnReadComplete().
  TRACE_EVENT_ASYNC_END2("media", "MojoCdmFileIO::Read", this, "bytes_read",
                         data.size(), "status", ConvertFileStatus(status));

  if (status != FileStatus::kSuccess) {
    DVLOG(1) << "Failed to read file " << file_name_;
    state_ = State::kOpened;
    OnError(ErrorType::kReadError);
    return;
  }

  // Call this before OnReadComplete() so that we always have the latest file
  // size before CDM fires errors.
  delegate_->ReportFileReadSize(data.size());

  state_ = State::kOpened;
  TRACE_EVENT0("media", "FileIOClient::OnReadComplete");
  client_->OnReadComplete(ClientStatus::kSuccess, data.data(), data.size());
}

void MojoCdmFileIO::Write(const uint8_t* data, uint32_t data_size) {
  DVLOG(3) << __func__ << " file: " << file_name_ << ", bytes: " << data_size;

  // If another operation is in progress, fail.
  if (state_ == State::kReading || state_ == State::kWriting) {
    OnError(ErrorType::kWriteInUse);
    return;
  }

  // If the file is not open, fail.
  if (state_ != State::kOpened) {
    OnError(ErrorType::kWriteError);
    return;
  }

  // Files are limited in size, so fail if file too big.
  if (data_size > kMaxFileSizeBytes) {
    DLOG(WARNING) << __func__
                  << " Too much data to write. #bytes = " << data_size;
    OnError(ErrorType::kWriteError);
    return;
  }

  UMA_HISTOGRAM_CUSTOM_COUNTS("Media.EME.CdmFileIO.WriteFile.DataSizeKB",
                              data_size / 1024, kSizeKBMin, kMaxFileSizeKB,
                              kSizeKBBuckets);

  TRACE_EVENT_ASYNC_BEGIN2("media", "MojoCdmFileIO::Write", this, "file_name",
                           file_name_, "bytes_to_write", data_size);

  state_ = State::kWriting;

  // Wrap the callback to detect the case when the mojo connection is
  // terminated prior to receiving the response. This avoids problems if the
  // service is destroyed before the CDM. If that happens let the CDM know that
  // Write() failed.
  auto callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&MojoCdmFileIO::OnFileWritten, weak_factory_.GetWeakPtr()),
      FileStatus::kFailure);
  cdm_file_->Write(std::vector<uint8_t>(data, data + data_size),
                   std::move(callback));
}

void MojoCdmFileIO::OnFileWritten(FileStatus status) {
  DVLOG(3) << __func__ << " file: " << file_name_;
  DCHECK_EQ(State::kWriting, state_);

  // This logs the end of the async Write() request, and separately logs
  // how long the client takes in OnWriteComplete().
  TRACE_EVENT_ASYNC_END1("media", "MojoCdmFileIO::Write", this, "status",
                         ConvertFileStatus(status));

  if (status != FileStatus::kSuccess) {
    DVLOG(1) << "Failed to write file " << file_name_;
    state_ = State::kError;
    OnError(ErrorType::kWriteError);
    return;
  }

  state_ = State::kOpened;
  TRACE_EVENT0("media", "FileIOClient::OnWriteComplete");
  client_->OnWriteComplete(ClientStatus::kSuccess);
}

void MojoCdmFileIO::Close() {
  DVLOG(3) << __func__ << " file: " << file_name_;

  // Note: |this| could be deleted as part of this call.
  delegate_->CloseCdmFileIO(this);
}

void MojoCdmFileIO::OnError(ErrorType error) {
  DVLOG(3) << __func__ << " file: " << file_name_ << ", error: " << (int)error;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MojoCdmFileIO::NotifyClientOfError,
                                weak_factory_.GetWeakPtr(), error));
}

void MojoCdmFileIO::NotifyClientOfError(ErrorType error) {
  // Note that no event tracing is done for error conditions.
  switch (error) {
    case ErrorType::kOpenError:
      client_->OnOpenComplete(ClientStatus::kError);
      break;
    case ErrorType::kOpenInUse:
      client_->OnOpenComplete(ClientStatus::kInUse);
      break;
    case ErrorType::kReadError:
      client_->OnReadComplete(ClientStatus::kError, nullptr, 0);
      break;
    case ErrorType::kReadInUse:
      client_->OnReadComplete(ClientStatus::kInUse, nullptr, 0);
      break;
    case ErrorType::kWriteError:
      client_->OnWriteComplete(ClientStatus::kError);
      break;
    case ErrorType::kWriteInUse:
      client_->OnWriteComplete(ClientStatus::kInUse);
      break;
  }
}

}  // namespace media
