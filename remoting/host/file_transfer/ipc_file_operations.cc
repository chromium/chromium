// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ipc_file_operations.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

class IpcFileOperations::IpcReader : public FileOperations::Reader {
 public:
  IpcReader(std::uint64_t file_id, base::WeakPtr<SharedState> shared_state);
  ~IpcReader() override;

  // FileOperations::Reader implementation.
  void Open(OpenCallback callback) override;
  void ReadChunk(std::size_t size, ReadCallback callback) override;
  const base::FilePath& filename() const override;
  std::uint64_t size() const override;
  State state() const override;

 private:
  void OnOpenResult(OpenCallback callback, ResultHandler::InfoResult result);
  void OnReadResult(ReadCallback callback, ResultHandler::DataResult result);

  State state_ = kCreated;
  std::uint64_t file_id_;
  base::FilePath filename_;
  std::uint64_t size_ = 0;
  base::WeakPtr<SharedState> shared_state_;

  DISALLOW_COPY_AND_ASSIGN(IpcReader);
};

class IpcFileOperations::IpcWriter : public FileOperations::Writer {
 public:
  IpcWriter(std::uint64_t file_id, base::WeakPtr<SharedState> shared_state);
  ~IpcWriter() override;

  // FileOperations::Writer implementation.
  void Open(const base::FilePath& filename, Callback callback) override;
  void WriteChunk(std::string data, Callback callback) override;
  void Close(Callback callback) override;
  State state() const override;

 private:
  void OnOperationResult(Callback callback, ResultHandler::Result result);
  void OnCloseResult(Callback callback, ResultHandler::Result result);

  State state_ = kCreated;
  std::uint64_t file_id_;
  base::WeakPtr<SharedState> shared_state_;

  DISALLOW_COPY_AND_ASSIGN(IpcWriter);
};

IpcFileOperations::IpcFileOperations(base::WeakPtr<SharedState> shared_state)
    : shared_state_(std::move(shared_state)) {}

IpcFileOperations::~IpcFileOperations() = default;

std::unique_ptr<FileOperations::Reader> IpcFileOperations::CreateReader() {
  return std::make_unique<IpcReader>(GetNextFileId(), shared_state_);
}

std::unique_ptr<FileOperations::Writer> IpcFileOperations::CreateWriter() {
  return std::make_unique<IpcWriter>(GetNextFileId(), shared_state_);
}

std::uint64_t IpcFileOperations::GetNextFileId() {
  // If shared_state_ is invalid, it means the connection is being torn down.
  // Using a dummy id is okay in that case, as the IpcReader/IpcWriter won't
  // actually do anything with an invalid shared_state_, and our call should be
  // torn down soon, as well.
  return shared_state_ ? shared_state_->next_file_id++ : 0;
}

IpcFileOperations::SharedState::SharedState(RequestHandler* request_handler)
    : request_handler(request_handler) {}

void IpcFileOperations::SharedState::Abort(std::uint64_t file_id) {
  request_handler->Cancel(file_id);

  protocol::FileTransfer_Error error = protocol::MakeFileTransferError(
      FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);

  // Any given file_id is expected to have at most one callback at a time, so
  // the order in which we search the maps is arbitrary.

  auto callback_iter = result_callbacks.find(file_id);
  if (callback_iter != result_callbacks.end()) {
    IpcFileOperations::ResultCallback callback =
        std::move(callback_iter->second);
    result_callbacks.erase(callback_iter);
    std::move(callback).Run(error);
  }

  auto info_callback_iter = info_result_callbacks.find(file_id);
  if (info_callback_iter != info_result_callbacks.end()) {
    IpcFileOperations::InfoResultCallback info_callback =
        std::move(info_callback_iter->second);
    info_result_callbacks.erase(info_callback_iter);
    std::move(info_callback).Run(error);
  }

  auto data_callback_iter = data_result_callbacks.find(file_id);
  if (data_callback_iter != data_result_callbacks.end()) {
    IpcFileOperations::DataResultCallback data_callback =
        std::move(data_callback_iter->second);
    data_result_callbacks.erase(data_callback_iter);
    std::move(data_callback).Run(error);
  }
}

IpcFileOperations::SharedState::~SharedState() = default;

IpcFileOperationsFactory::IpcFileOperationsFactory(
    IpcFileOperations::RequestHandler* request_handler)
    : shared_state_(request_handler) {}

IpcFileOperationsFactory::~IpcFileOperationsFactory() = default;

std::unique_ptr<FileOperations>
IpcFileOperationsFactory::CreateFileOperations() {
  return base::WrapUnique(
      new IpcFileOperations(shared_state_.weak_ptr_factory.GetWeakPtr()));
}

void IpcFileOperationsFactory::OnResult(uint64_t file_id, Result result) {
  auto callback_iter = shared_state_.result_callbacks.find(file_id);
  if (callback_iter == shared_state_.result_callbacks.end()) {
    shared_state_.Abort(file_id);
    return;
  }

  IpcFileOperations::ResultCallback callback = std::move(callback_iter->second);
  shared_state_.result_callbacks.erase(callback_iter);
  std::move(callback).Run(std::move(result));
}

void IpcFileOperationsFactory::OnInfoResult(std::uint64_t file_id,
                                            InfoResult result) {
  auto callback_iter = shared_state_.info_result_callbacks.find(file_id);
  if (callback_iter == shared_state_.info_result_callbacks.end()) {
    shared_state_.Abort(file_id);
    return;
  }

  IpcFileOperations::InfoResultCallback callback =
      std::move(callback_iter->second);
  shared_state_.info_result_callbacks.erase(callback_iter);
  std::move(callback).Run(std::move(result));
}

void IpcFileOperationsFactory::OnDataResult(std::uint64_t file_id,
                                            DataResult result) {
  auto callback_iter = shared_state_.data_result_callbacks.find(file_id);
  if (callback_iter == shared_state_.data_result_callbacks.end()) {
    shared_state_.Abort(file_id);
    return;
  }

  IpcFileOperations::DataResultCallback callback =
      std::move(callback_iter->second);
  shared_state_.data_result_callbacks.erase(callback_iter);
  std::move(callback).Run(std::move(result));
}

IpcFileOperations::IpcReader::IpcReader(std::uint64_t file_id,
                                        base::WeakPtr<SharedState> shared_state)
    : file_id_(file_id), shared_state_(std::move(shared_state)) {}

IpcFileOperations::IpcReader::~IpcReader() {
  if (!shared_state_ || state_ == kCreated || state_ == kComplete ||
      state_ == kFailed) {
    return;
  }

  shared_state_->request_handler->Cancel(file_id_);

  // Destroy any pending callbacks.
  auto info_callback_iter = shared_state_->info_result_callbacks.find(file_id_);
  if (info_callback_iter != shared_state_->info_result_callbacks.end()) {
    shared_state_->info_result_callbacks.erase(info_callback_iter);
  }

  auto data_callback_iter = shared_state_->data_result_callbacks.find(file_id_);
  if (data_callback_iter != shared_state_->data_result_callbacks.end()) {
    shared_state_->data_result_callbacks.erase(data_callback_iter);
  }
}

void IpcFileOperations::IpcReader::Open(OpenCallback callback) {
  DCHECK_EQ(kCreated, state_);
  if (!shared_state_) {
    return;
  }

  state_ = kBusy;
  // Unretained is sound because we destroy any pending callbacks in our
  // destructor.
  shared_state_->info_result_callbacks.emplace(
      file_id_, base::BindOnce(&IpcReader::OnOpenResult, base::Unretained(this),
                               std::move(callback)));
  shared_state_->request_handler->ReadFile(file_id_);
}

void IpcFileOperations::IpcReader::ReadChunk(
    std::size_t size,
    FileOperations::Reader::ReadCallback callback) {
  DCHECK_EQ(kReady, state_);
  if (!shared_state_) {
    return;
  }

  state_ = kBusy;
  // Unretained is sound because we destroy any pending callbacks in our
  // destructor.
  shared_state_->data_result_callbacks.emplace(
      file_id_, base::BindOnce(&IpcReader::OnReadResult, base::Unretained(this),
                               std::move(callback)));
  shared_state_->request_handler->ReadChunk(file_id_, size);
}

const base::FilePath& IpcFileOperations::IpcReader::filename() const {
  return filename_;
}

std::uint64_t IpcFileOperations::IpcReader::size() const {
  return size_;
}

FileOperations::State IpcFileOperations::IpcReader::state() const {
  return state_;
}

void IpcFileOperations::IpcReader::OnOpenResult(
    OpenCallback callback,
    ResultHandler::InfoResult result) {
  if (!result) {
    state_ = kFailed;
    std::move(callback).Run(result.error());
    return;
  }

  state_ = kReady;
  filename_ = std::move(std::get<0>(*result));
  size_ = std::move(std::get<1>(*result));
  std::move(callback).Run(kSuccessTag);
}

void IpcFileOperations::IpcReader::OnReadResult(
    ReadCallback callback,
    ResultHandler::DataResult result) {
  if (result) {
    state_ = result->size() == 0 ? kComplete : kReady;
  } else {
    state_ = kFailed;
  }
  std::move(callback).Run(std::move(result));
}

IpcFileOperations::IpcWriter::IpcWriter(std::uint64_t file_id,
                                        base::WeakPtr<SharedState> shared_state)
    : file_id_(file_id), shared_state_(std::move(shared_state)) {}

IpcFileOperations::IpcWriter::~IpcWriter() {
  if (!shared_state_ || state_ == kCreated || state_ == kComplete ||
      state_ == kFailed) {
    return;
  }

  shared_state_->request_handler->Cancel(file_id_);

  // Destroy any pending callbacks.
  auto callback_iter = shared_state_->result_callbacks.find(file_id_);
  if (callback_iter != shared_state_->result_callbacks.end()) {
    shared_state_->result_callbacks.erase(callback_iter);
  }
}

void IpcFileOperations::IpcWriter::Open(const base::FilePath& filename,
                                        Callback callback) {
  DCHECK_EQ(kCreated, state_);
  if (!shared_state_) {
    return;
  }

  state_ = kBusy;
  shared_state_->result_callbacks.emplace(
      file_id_, base::BindOnce(&IpcWriter::OnOperationResult,
                               base::Unretained(this), std::move(callback)));
  shared_state_->request_handler->WriteFile(file_id_, filename);
}

void IpcFileOperations::IpcWriter::WriteChunk(std::string data,
                                              Callback callback) {
  DCHECK_EQ(kReady, state_);
  if (!shared_state_) {
    return;
  }

  state_ = kBusy;
  // Unretained is sound because IpcWriter will destroy any outstanding callback
  // in its destructor.
  shared_state_->result_callbacks.emplace(
      file_id_, base::BindOnce(&IpcWriter::OnOperationResult,
                               base::Unretained(this), std::move(callback)));
  shared_state_->request_handler->WriteChunk(file_id_, data);
}

void IpcFileOperations::IpcWriter::Close(Callback callback) {
  DCHECK_EQ(kReady, state_);
  if (!shared_state_) {
    return;
  }

  state_ = kBusy;
  shared_state_->request_handler->Close(file_id_);
  // Unretained is sound because IpcWriter will destroy any outstanding callback
  // in its destructor.
  shared_state_->result_callbacks.emplace(
      file_id_, base::BindOnce(&IpcWriter::OnCloseResult,
                               base::Unretained(this), std::move(callback)));
}

FileOperations::State IpcFileOperations::IpcWriter::state() const {
  return state_;
}

void IpcFileOperations::IpcWriter::OnOperationResult(
    Callback callback,
    ResultHandler::Result result) {
  if (result) {
    state_ = kReady;
  } else {
    state_ = kFailed;
  }
  std::move(callback).Run(std::move(result));
}

void IpcFileOperations::IpcWriter::OnCloseResult(Callback callback,
                                                 ResultHandler::Result result) {
  if (result) {
    state_ = kComplete;
  } else {
    state_ = kFailed;
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace remoting
