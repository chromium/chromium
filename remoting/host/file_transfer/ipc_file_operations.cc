// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ipc_file_operations.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

// This is an overview of how IpcFileOperations is integrated and used in the
// multi-process host architecture. Reasoning about the lifetime and ownership
// of the various pieces currently requires digging through the code so this
// comment block describes the relationships and pieces involved at a high-level
// to help those looking to understand the code.
//
// The IpcFileOperations and related classes are all used in the low-privilege
// network process. They handle network communication with the website client
// over a WebRTC data channel and proxy those requests using Mojo to the
// SessionFileOperationsHandler (and friends) which lives in the high-privilege
// desktop process and handles the actual file reading and writing.
//
// When a new file transfer data channel is opened by the client, the
// ClientSession instance on the host (running in the network process) will
// create a FileTransferMessageHandler (FTMH) instance to service it. As part of
// the FTMH creation, ClientSession will ask the IpcDesktopEnvironment to create
// a new IpcFileOperations instance. This instance will be provided with a
// WeakPtr<IpcFileOperations::RequestHandler> which is used to start a file read
// or write operation in the desktop process over an existing IPC channel owned
// by the DesktopSessionProxy.
//
// After the FTMH receives the initial message indicating the type of operation
// to perform, it creates an IpcFileReader or an IpcFileWriter instance. The
// IpcFile{Reader|Writer} begins an operation by calling the appropriate method
// on the IpcFileOperations::RequestHandler interface. This interface is
// implemented by the DesktopSessionProxy (DSP) which in turn calls the
// DesktopSessionAgent (DSA) via its mojom::DesktopSessionControl remote. The
// DSA passes the request to its SessionFileOperationsHandler instance which, if
// successful, will create a new IPC channel for the transfer and return a
// remote to the IpcFile{Reader|Writer} to allow it to proceed with the file
// operation. The receiver is owned by a MojoFileReader or MojoFileWriter
// instance whose lifetime is tied to the Mojo channel meaning the
// MojoFile{Reader|Writer} will be destroyed when the channel is disconnected.
//
// The lifetime of an FTMH instance is tied to the WebRTC file transfer data
// channel that it was created to service. Each data channel exists for one
// transfer request, so once the operation completes, or encounters an error,
// the IpcFileOperations instance and the IpcFile{Reader|Writer} it created will
// be destroyed (this will also trigger destruction of a MojoFile{Reader|Writer}
// in the desktop process).
//
// The lifetime of the DesktopSessionProxy is a bit harder to reason about as a
// number of classes and callbacks hold a scoped_refptr reference to it. At the
// very earliest, the DSP will be destroyed when the chromoting session is
// terminated. When this occurs, the scoped_refptr in ClientSession is released
// and the IpcDesktopEnvironment and IpcFileOperationsFactory are destroyed.
//
// Because of the objects involved, the two UaF concerns are:
// - Calling into |request_handler_| after the DSP has been destroyed.
//   This is unlikely given that a DSP lasts for the entire session but it
//   could occur if the timing was just right near the end of a session.
//   Mitigation: |request_handler_| is wrapped in a WeakPtr and provided to each
//               IpcFile{Reader|Writer} instance.
// - The DSP could invoke a disconnect_handler on the IpcFile{Reader|Writer} if
//   the file transfer request was canceled just after the operation started.
//   Mitigation: The disconnect_handler callback provided to the BeginFileRead
//               BeginFileWrite method is bound with a WeakPtr.
class IpcFileOperations::IpcReader : public FileOperations::Reader {
 public:
  explicit IpcReader(base::WeakPtr<RequestHandler> request_handler);

  IpcReader(const IpcReader&) = delete;
  IpcReader& operator=(const IpcReader&) = delete;

  ~IpcReader() override;

  // FileOperations::Reader implementation.
  void Open(OpenCallback callback) override;
  void ReadChunk(std::size_t size, ReadCallback callback) override;
  const base::FilePath& filename() const override;
  std::uint64_t size() const override;
  State state() const override;

  void OnChannelDisconnected();

  base::WeakPtr<IpcReader> GetWeakPtr();

 private:
  void OnOpenResult(mojom::BeginFileReadResultPtr result);
  void OnReadResult(
      const protocol::FileTransferResult<std::vector<std::uint8_t>>& result);

  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = kCreated;
  base::FilePath filename_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::uint64_t size_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  OpenCallback pending_open_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  ReadCallback pending_read_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtr<IpcFileOperations::RequestHandler> request_handler_;
  mojo::AssociatedRemote<mojom::FileReader> remote_file_reader_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IpcReader> weak_ptr_factory_{this};
};

class IpcFileOperations::IpcWriter : public FileOperations::Writer {
 public:
  explicit IpcWriter(base::WeakPtr<RequestHandler> request_handler);

  IpcWriter(const IpcWriter&) = delete;
  IpcWriter& operator=(const IpcWriter&) = delete;

  ~IpcWriter() override;

  // FileOperations::Writer implementation.
  void Open(const base::FilePath& filename, Callback callback) override;
  void WriteChunk(std::vector<std::uint8_t> data, Callback callback) override;
  void Close(Callback callback) override;
  State state() const override;

  void OnChannelDisconnected();

  base::WeakPtr<IpcWriter> GetWeakPtr();

 private:
  void OnOpenResult(mojom::BeginFileWriteResultPtr result);
  void OnOperationResult(
      const std::optional<::remoting::protocol::FileTransfer_Error>& error);
  void OnCloseResult(
      const std::optional<::remoting::protocol::FileTransfer_Error>& error);

  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = kCreated;
  Callback pending_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtr<IpcFileOperations::RequestHandler> request_handler_;
  mojo::AssociatedRemote<mojom::FileWriter> remote_file_writer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IpcWriter> weak_ptr_factory_{this};
};

IpcFileOperations::IpcFileOperations(
    base::WeakPtr<RequestHandler> request_handler)
    : request_handler_(std::move(request_handler)) {}

IpcFileOperations::~IpcFileOperations() = default;

std::unique_ptr<FileOperations::Reader> IpcFileOperations::CreateReader() {
  return std::make_unique<IpcReader>(request_handler_);
}

std::unique_ptr<FileOperations::Writer> IpcFileOperations::CreateWriter() {
  return std::make_unique<IpcWriter>(request_handler_);
}

IpcFileOperationsFactory::IpcFileOperationsFactory(
    IpcFileOperations::RequestHandler* request_handler)
    : request_handler_weak_ptr_factory_(request_handler) {}

IpcFileOperationsFactory::~IpcFileOperationsFactory() = default;

std::unique_ptr<FileOperations>
IpcFileOperationsFactory::CreateFileOperations() {
  return base::WrapUnique(
      new IpcFileOperations(request_handler_weak_ptr_factory_.GetWeakPtr()));
}

IpcFileOperations::IpcReader::IpcReader(
    base::WeakPtr<RequestHandler> request_handler)
    : request_handler_(std::move(request_handler)) {}

IpcFileOperations::IpcReader::~IpcReader() = default;

void IpcFileOperations::IpcReader::Open(OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(kCreated, state_);

  if (!request_handler_) {
    state_ = kFailed;
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }

  state_ = kBusy;
  pending_open_callback_ = std::move(callback);
  request_handler_->BeginFileRead(
      base::BindOnce(&IpcReader::OnOpenResult, GetWeakPtr()),
      base::BindOnce(&IpcReader::OnChannelDisconnected, GetWeakPtr()));
}

void IpcFileOperations::IpcReader::ReadChunk(std::size_t size,
                                             ReadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != kReady || !remote_file_reader_.is_connected()) {
    state_ = kFailed;
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }

  state_ = kBusy;
  pending_read_callback_ = std::move(callback);
  // Unretained is sound because the remote is owned by this instance and will
  // be destroyed at the same time which will clear any callbacks.
  remote_file_reader_->ReadChunk(
      size, base::BindOnce(&IpcReader::OnReadResult, base::Unretained(this)));
}

const base::FilePath& IpcFileOperations::IpcReader::filename() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return filename_;
}

std::uint64_t IpcFileOperations::IpcReader::size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return size_;
}

FileOperations::State IpcFileOperations::IpcReader::state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

void IpcFileOperations::IpcReader::OnChannelDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = kFailed;

  if (pending_open_callback_) {
    std::move(pending_open_callback_)
        .Run(protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  } else if (pending_read_callback_) {
    std::move(pending_read_callback_)
        .Run(protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  }
}

base::WeakPtr<IpcFileOperations::IpcReader>
IpcFileOperations::IpcReader::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void IpcFileOperations::IpcReader::OnOpenResult(
    mojom::BeginFileReadResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result->is_error()) {
    state_ = kFailed;
    std::move(pending_open_callback_).Run(result->get_error());
    return;
  }

  state_ = kReady;
  auto& success_ptr = result->get_success();
  filename_ = std::move(success_ptr->filename);
  size_ = success_ptr->size;

  remote_file_reader_.Bind(std::move(success_ptr->file_reader));
  // base::Unretained is sound because this instance owns |remote_file_reader_|
  // and the handler will not run after it is destroyed.
  remote_file_reader_.set_disconnect_handler(base::BindOnce(
      &IpcReader::OnChannelDisconnected, base::Unretained(this)));

  std::move(pending_open_callback_).Run(kSuccessTag);
}

void IpcFileOperations::IpcReader::OnReadResult(
    const protocol::FileTransferResult<std::vector<std::uint8_t>>& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result) {
    state_ = result->size() == 0 ? kComplete : kReady;
  } else {
    state_ = kFailed;
  }

  if (state_ != kReady) {
    // Don't need the remote if we're done or an error occurred.
    remote_file_reader_.reset();
  }

  std::move(pending_read_callback_).Run(std::move(result));
}

IpcFileOperations::IpcWriter::IpcWriter(
    base::WeakPtr<RequestHandler> request_handler)
    : request_handler_(std::move(request_handler)) {}

IpcFileOperations::IpcWriter::~IpcWriter() = default;

void IpcFileOperations::IpcWriter::Open(const base::FilePath& filename,
                                        Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(kCreated, state_);

  if (!request_handler_) {
    state_ = kFailed;
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }

  state_ = kBusy;
  pending_callback_ = std::move(callback);
  request_handler_->BeginFileWrite(
      filename, base::BindOnce(&IpcWriter::OnOpenResult, GetWeakPtr()),
      base::BindOnce(&IpcWriter::OnChannelDisconnected, GetWeakPtr()));
}

void IpcFileOperations::IpcWriter::WriteChunk(std::vector<std::uint8_t> data,
                                              Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != kReady) {
    state_ = kFailed;
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }

  state_ = kBusy;
  pending_callback_ = std::move(callback);
  // Unretained is sound because the remote is owned by this instance and will
  // be destroyed at the same time which will clear this callback.
  remote_file_writer_->WriteChunk(
      std::move(data),
      base::BindOnce(&IpcWriter::OnOperationResult, base::Unretained(this)));
}

void IpcFileOperations::IpcWriter::Close(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != kReady) {
    state_ = kFailed;
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }

  state_ = kBusy;
  pending_callback_ = std::move(callback);
  // Unretained is sound because the remote is owned by this instance and will
  // be destroyed at the same time which will clear this callback.
  remote_file_writer_->CloseFile(
      base::BindOnce(&IpcWriter::OnCloseResult, base::Unretained(this)));
}

FileOperations::State IpcFileOperations::IpcWriter::state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

void IpcFileOperations::IpcWriter::OnChannelDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_ = kFailed;

  if (pending_callback_) {
    std::move(pending_callback_)
        .Run(protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  }
}

base::WeakPtr<IpcFileOperations::IpcWriter>
IpcFileOperations::IpcWriter::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void IpcFileOperations::IpcWriter::OnOpenResult(
    mojom::BeginFileWriteResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result->is_error()) {
    state_ = kFailed;
    std::move(pending_callback_).Run(result->get_error());
    return;
  }

  state_ = kReady;
  auto& success_ptr = result->get_success();
  remote_file_writer_.Bind(std::move(success_ptr->file_writer));
  // base::Unretained is sound because this instance owns |remote_file_writer_|
  // and the handler will not run after it is destroyed.
  remote_file_writer_.set_disconnect_handler(base::BindOnce(
      &IpcWriter::OnChannelDisconnected, base::Unretained(this)));

  std::move(pending_callback_).Run(kSuccessTag);
}

void IpcFileOperations::IpcWriter::OnOperationResult(
    const std::optional<::remoting::protocol::FileTransfer_Error>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    state_ = kFailed;
    std::move(pending_callback_).Run(std::move(*error));
    remote_file_writer_.reset();
    return;
  }

  state_ = kReady;
  std::move(pending_callback_).Run({kSuccessTag});
}

void IpcFileOperations::IpcWriter::OnCloseResult(
    const std::optional<::remoting::protocol::FileTransfer_Error>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We're done with the remote regardless of the result.
  remote_file_writer_.reset();

  if (error) {
    state_ = kFailed;
    std::move(pending_callback_).Run(std::move(*error));
  } else {
    state_ = kComplete;
    std::move(pending_callback_).Run({kSuccessTag});
  }
}

}  // namespace remoting
