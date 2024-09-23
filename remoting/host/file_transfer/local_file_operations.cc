// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/local_file_operations.h"

#include <cstdint>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_proxy.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "remoting/base/result.h"
#include "remoting/host/file_transfer/directory_helpers.h"
#include "remoting/host/file_transfer/ensure_user.h"
#include "remoting/host/file_transfer/file_chooser.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

namespace {

constexpr char kTempFileExtension[] = ".part";

remoting::protocol::FileTransfer_Error_Type FileErrorToResponseErrorType(
    base::File::Error file_error) {
  switch (file_error) {
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return remoting::protocol::FileTransfer_Error_Type_PERMISSION_DENIED;
    case base::File::FILE_ERROR_NO_SPACE:
      return remoting::protocol::FileTransfer_Error_Type_OUT_OF_DISK_SPACE;
    default:
      return remoting::protocol::FileTransfer_Error_Type_IO_ERROR;
  }
}

scoped_refptr<base::SequencedTaskRunner> CreateFileTaskRunner() {
#if BUILDFLAG(IS_WIN)
  // On Windows, we use user impersonation to write files as the currently
  // logged-in user, while the process as a whole runs as SYSTEM. Since user
  // impersonation is per-thread on Windows, we need a dedicated thread to
  // ensure that no other code is accidentally run with the wrong privileges.
  return base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
#else
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
#endif
}

class LocalFileReader : public FileOperations::Reader {
 public:
  explicit LocalFileReader(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  LocalFileReader(const LocalFileReader&) = delete;
  LocalFileReader& operator=(const LocalFileReader&) = delete;

  ~LocalFileReader() override;

  // FileOperations::Reader implementation.
  void Open(OpenCallback callback) override;
  void ReadChunk(std::size_t size, ReadCallback callback) override;
  const base::FilePath& filename() const override;
  std::uint64_t size() const override;
  FileOperations::State state() const override;

 private:
  void OnEnsureUserResult(OpenCallback callback,
                          protocol::FileTransferResult<absl::monostate> result);
  void OnFileChooserResult(OpenCallback callback, FileChooser::Result result);
  void OnOpenResult(OpenCallback callback, base::File::Error error);
  void OnGetInfoResult(OpenCallback callback,
                       base::File::Error error,
                       const base::File::Info& info);
  void OnReadResult(ReadCallback callback,
                    base::File::Error error,
                    base::span<const char> data);

  void SetState(FileOperations::State state);

  base::FilePath filename_;
  std::uint64_t size_ = 0;
  std::uint64_t offset_ = 0;
  FileOperations::State state_ = FileOperations::kCreated;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  std::unique_ptr<FileChooser> file_chooser_;
  std::optional<base::FileProxy> file_proxy_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LocalFileReader> weak_ptr_factory_{this};
};

class LocalFileWriter : public FileOperations::Writer {
 public:
  LocalFileWriter();

  LocalFileWriter(const LocalFileWriter&) = delete;
  LocalFileWriter& operator=(const LocalFileWriter&) = delete;

  ~LocalFileWriter() override;

  // FileOperations::Writer implementation.
  void Open(const base::FilePath& filename, Callback callback) override;
  void WriteChunk(std::vector<std::uint8_t> data, Callback callback) override;
  void Close(Callback callback) override;
  FileOperations::State state() const override;

 private:
  void Cancel();

  // Callbacks for Open().
  void OnGetTargetDirectoryResult(
      base::FilePath filename,
      Callback callback,
      protocol::FileTransferResult<base::FilePath> target_directory_result);
  void CreateTempFile(Callback callback, base::FilePath temp_filepath);
  void OnCreateResult(Callback callback, base::File::Error error);

  void OnWriteResult(std::vector<std::uint8_t> data,
                     Callback callback,
                     base::File::Error error,
                     int bytes_written);

  // Callbacks for Close().
  void OnCloseResult(Callback callback, base::File::Error error);
  void MoveToDestination(Callback callback,
                         base::FilePath destination_filepath);
  void OnMoveResult(Callback callback, bool success);

  void SetState(FileOperations::State state);

  FileOperations::State state_ = FileOperations::kCreated;

  base::FilePath destination_filepath_;
  base::FilePath temp_filepath_;
  std::uint64_t bytes_written_ = 0;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  std::optional<base::FileProxy> file_proxy_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LocalFileWriter> weak_ptr_factory_{this};
};

LocalFileReader::LocalFileReader(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)) {}

LocalFileReader::~LocalFileReader() = default;

void LocalFileReader::Open(OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(FileOperations::kCreated, state_);
  SetState(FileOperations::kBusy);
  file_task_runner_ = CreateFileTaskRunner();
  file_proxy_.emplace(file_task_runner_.get());
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&EnsureUserContext),
      base::BindOnce(&LocalFileReader::OnEnsureUserResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileReader::ReadChunk(std::size_t size, ReadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(FileOperations::kReady, state_);
  SetState(FileOperations::kBusy);
  file_proxy_->Read(
      offset_, size,
      base::BindOnce(&LocalFileReader::OnReadResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

const base::FilePath& LocalFileReader::filename() const {
  return filename_;
}

uint64_t LocalFileReader::size() const {
  return size_;
}

FileOperations::State LocalFileReader::state() const {
  return state_;
}

void LocalFileReader::OnEnsureUserResult(
    FileOperations::Reader::OpenCallback callback,
    protocol::FileTransferResult<absl::monostate> result) {
  if (!result) {
    SetState(FileOperations::kFailed);
    std::move(callback).Run(std::move(result.error()));
    return;
  }

  file_chooser_ = FileChooser::Create(
      ui_task_runner_,
      base::BindOnce(&LocalFileReader::OnFileChooserResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  file_chooser_->Show();
}

void LocalFileReader::OnFileChooserResult(OpenCallback callback,
                                          FileChooser::Result result) {
  file_chooser_.reset();
  if (!result) {
    SetState(FileOperations::kFailed);
    std::move(callback).Run(std::move(result.error()));
    return;
  }

  filename_ = result->BaseName();
  file_proxy_->CreateOrOpen(
      *result, base::File::FLAG_OPEN | base::File::FLAG_READ,
      base::BindOnce(&LocalFileReader::OnOpenResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileReader::OnOpenResult(OpenCallback callback,
                                   base::File::Error error) {
  if (error != base::File::FILE_OK) {
    SetState(FileOperations::kFailed);
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, FileErrorToResponseErrorType(error), error));
    return;
  }

  file_proxy_->GetInfo(base::BindOnce(&LocalFileReader::OnGetInfoResult,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(callback)));
}

void LocalFileReader::OnGetInfoResult(OpenCallback callback,
                                      base::File::Error error,
                                      const base::File::Info& info) {
  if (error != base::File::FILE_OK) {
    SetState(FileOperations::kFailed);
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, FileErrorToResponseErrorType(error), error));
    return;
  }

  size_ = info.size;

  SetState(FileOperations::kReady);
  std::move(callback).Run(kSuccessTag);
}

void LocalFileReader::OnReadResult(ReadCallback callback,
                                   base::File::Error error,
                                   base::span<const char> data) {
  if (error != base::File::FILE_OK) {
    SetState(FileOperations::kFailed);
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, FileErrorToResponseErrorType(error), error));
    return;
  }

  offset_ += data.size();
  SetState(data.size() > 0 ? FileOperations::kReady
                           : FileOperations::kComplete);

  // The read buffer is provided and owned by FileProxy, so there's no way to
  // avoid a copy, here.
  std::move(callback).Run(std::vector<std::uint8_t>(data.begin(), data.end()));
}

void LocalFileReader::SetState(FileOperations::State state) {
  switch (state) {
    case FileOperations::kCreated:
      NOTREACHED();  // Can never return to initial state.
    case FileOperations::kReady:
      DCHECK_EQ(FileOperations::kBusy, state_);
      break;
    case FileOperations::kBusy:
      DCHECK(state_ == FileOperations::kCreated ||
             state_ == FileOperations::kReady);
      break;
    case FileOperations::kComplete:
      DCHECK_EQ(FileOperations::kBusy, state_);
      break;
    case FileOperations::kFailed:
      // Any state can change to kFailed.
      break;
  }

  state_ = state;
}

LocalFileWriter::LocalFileWriter() {}

LocalFileWriter::~LocalFileWriter() {
  Cancel();
}

void LocalFileWriter::Open(const base::FilePath& filename, Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(FileOperations::kCreated, state_);
  SetState(FileOperations::kBusy);
  file_task_runner_ = CreateFileTaskRunner();
  file_proxy_.emplace(file_task_runner_.get());
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce([] {
        return EnsureUserContext().AndThen(
            [](absl::monostate) { return GetFileUploadDirectory(); });
      }),
      base::BindOnce(&LocalFileWriter::OnGetTargetDirectoryResult,
                     weak_ptr_factory_.GetWeakPtr(), filename,
                     std::move(callback)));
}

void LocalFileWriter::WriteChunk(std::vector<std::uint8_t> data,
                                 Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(FileOperations::kReady, state_);
  SetState(FileOperations::kBusy);
  // TODO(rkjnsn): Under what circumstances can posting the task fail? Is it
  //               worth checking for? If so, what should we do in that case,
  //               given that callback is moved into the task and not returned
  //               on error?
  // Ensure span is obtained before data is moved.
  auto data_span = base::make_span(data);
  file_proxy_->Write(bytes_written_, data_span,
                     base::BindOnce(&LocalFileWriter::OnWriteResult,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(data), std::move(callback)));
}

void LocalFileWriter::Close(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(FileOperations::kReady, state_);
  SetState(FileOperations::kBusy);
  file_proxy_->Close(base::BindOnce(&LocalFileWriter::OnCloseResult,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
}

void LocalFileWriter::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == FileOperations::kCreated ||
      state_ == FileOperations::kComplete ||
      state_ == FileOperations::kFailed) {
    return;
  }

  // Ensure we don't receive further callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Drop FileProxy, which will close the underlying file on the file
  // sequence after any possible pending operation is complete.
  file_proxy_.reset();
  // And finally, queue deletion of the temp file.
  if (!temp_filepath_.empty()) {
    file_task_runner_->PostTask(FROM_HERE,
                                base::GetDeleteFileCallback(temp_filepath_));
  }
  SetState(FileOperations::kFailed);
}

FileOperations::State LocalFileWriter::state() const {
  return state_;
}

void LocalFileWriter::OnGetTargetDirectoryResult(
    base::FilePath filename,
    Callback callback,
    protocol::FileTransferResult<base::FilePath> target_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!target_directory) {
    LOG(ERROR) << "Failed to get target directory.";
    SetState(FileOperations::kFailed);
    std::move(callback).Run(std::move(target_directory.error()));
    return;
  }

  destination_filepath_ =
      target_directory.success().Append(filename.BaseName());
  // Don't store in temp_filepath_ until we have the final path to make sure
  // Cancel can never delete the wrong file.
  base::FilePath temp_filepath =
      destination_filepath_.AddExtensionASCII(kTempFileExtension);

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::GetUniquePath, temp_filepath),
      base::BindOnce(&LocalFileWriter::CreateTempFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileWriter::CreateTempFile(Callback callback,
                                     base::FilePath temp_filepath) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (temp_filepath.empty()) {
    LOG(ERROR) << "Failed to get unique path number.";
    SetState(FileOperations::kFailed);
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR));
    return;
  }

  temp_filepath_ = std::move(temp_filepath);

  // FLAG_WIN_SHARE_DELETE allows the file to be marked as deleted on Windows
  // while the handle is still open. (Other OS's allow this by default.) This
  // allows Cancel to clean up the temporary file even if there are writes
  // pending.
  file_proxy_->CreateOrOpen(
      temp_filepath_,
      base::File::FLAG_CREATE | base::File::FLAG_WRITE |
          base::File::FLAG_WIN_SHARE_DELETE,
      base::BindOnce(&LocalFileWriter::OnCreateResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileWriter::OnCreateResult(Callback callback,
                                     base::File::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Creating temp file failed with error: " << error;
    SetState(FileOperations::kFailed);
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, FileErrorToResponseErrorType(error), error));
    return;
  }

  SetState(FileOperations::kReady);
  // Now that the temp file has been created successfully, we could lock it
  // using base::File::Lock(), but this would not prevent the file from being
  // deleted. When the file is deleted, WriteChunk() will continue to write to
  // the file as if the file was still there, and an error will occur when
  // calling base::Move() to move the temp file. Chrome exhibits the same
  // behavior with its downloads.
  std::move(callback).Run(kSuccessTag);
}

void LocalFileWriter::OnWriteResult(std::vector<std::uint8_t> data,
                                    Callback callback,
                                    base::File::Error error,
                                    int bytes_written) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Write failed with error: " << error;
    Cancel();
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, FileErrorToResponseErrorType(error), error));
    return;
  }

  SetState(FileOperations::kReady);
  bytes_written_ += bytes_written;

  // bytes_written should never be negative if error is FILE_OK.
  if (static_cast<std::size_t>(bytes_written) != data.size()) {
    // Write already makes a "best effort" to write all of the data, so this
    // probably means that an error occurred. Unfortunately, the only way to
    // find out what went wrong is to try again.
    // TODO(rkjnsn): Would it be better just to return a generic error, here?
    WriteChunk(
        std::vector<std::uint8_t>(data.begin() + bytes_written, data.end()),
        std::move(callback));
    return;
  }

  std::move(callback).Run(kSuccessTag);
}

void LocalFileWriter::OnCloseResult(Callback callback,
                                    base::File::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Close failed with error: " << error;
    Cancel();
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, FileErrorToResponseErrorType(error), error));
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::GetUniquePath, destination_filepath_),
      base::BindOnce(&LocalFileWriter::MoveToDestination,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileWriter::MoveToDestination(Callback callback,
                                        base::FilePath destination_filepath) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (destination_filepath.empty()) {
    LOG(ERROR) << "Failed to get unique path number.";
    SetState(FileOperations::kFailed);
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR));
    return;
  }

  destination_filepath_ = std::move(destination_filepath);

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::Move, temp_filepath_, destination_filepath_),
      base::BindOnce(&LocalFileWriter::OnMoveResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileWriter::OnMoveResult(Callback callback, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success) {
    SetState(FileOperations::kComplete);
    std::move(callback).Run(kSuccessTag);
  } else {
    LOG(ERROR) << "Failed to move file to final destination.";
    Cancel();
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR));
  }
}

void LocalFileWriter::SetState(FileOperations::State state) {
  switch (state) {
    case FileOperations::kCreated:
      NOTREACHED();  // Can never return to initial state.
    case FileOperations::kReady:
      DCHECK(state_ == FileOperations::kBusy);
      break;
    case FileOperations::kBusy:
      DCHECK(state_ == FileOperations::kCreated ||
             state_ == FileOperations::kReady);
      break;
    case FileOperations::kComplete:
      DCHECK(state_ == FileOperations::kBusy);
      break;
    case FileOperations::kFailed:
      // Any state can change to kFailed.
      break;
  }

  state_ = state;
}

}  // namespace

LocalFileOperations::LocalFileOperations(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner)) {}

LocalFileOperations::~LocalFileOperations() = default;

std::unique_ptr<FileOperations::Reader> LocalFileOperations::CreateReader() {
  return std::make_unique<LocalFileReader>(ui_task_runner_);
}

std::unique_ptr<FileOperations::Writer> LocalFileOperations::CreateWriter() {
  return std::make_unique<LocalFileWriter>();
}

}  // namespace remoting
