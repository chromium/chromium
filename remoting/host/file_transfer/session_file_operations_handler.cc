// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/session_file_operations_handler.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

namespace {

using BeginFileReadCallback =
    SessionFileOperationsHandler::BeginFileReadCallback;
using BeginFileWriteCallback =
    SessionFileOperationsHandler::BeginFileWriteCallback;

using Reader = FileOperations::Reader;
using Writer = FileOperations::Writer;

class MojoFileReader : public mojom::FileReader {
 public:
  explicit MojoFileReader(std::unique_ptr<Reader> file_reader);

  MojoFileReader(const MojoFileReader&) = delete;
  MojoFileReader& operator=(const MojoFileReader&) = delete;

  ~MojoFileReader() override = default;

  base::WeakPtr<MojoFileReader> GetWeakPtr();

  // Requests |file_reader_| to open an existing file for reading.
  void Open(Reader::OpenCallback callback);
  // Receives the result from Open() and returns it over the mojo channel.
  void OnFileOpened(BeginFileReadCallback callback,
                    mojo::PendingAssociatedRemote<mojom::FileReader> remote,
                    Reader::OpenResult result);

  // mojom::FileReader implementation.
  void ReadChunk(uint64_t bytes_to_read, ReadChunkCallback callback) override;

  const base::FilePath& filename() const { return file_reader_->filename(); }

  uint64_t size() const { return file_reader_->size(); }

 private:
  const std::unique_ptr<Reader> file_reader_;
  base::WeakPtrFactory<MojoFileReader> weak_ptr_factory{this};
};

class MojoFileWriter : public mojom::FileWriter {
 public:
  explicit MojoFileWriter(std::unique_ptr<Writer> file_writer);

  MojoFileWriter(const MojoFileWriter&) = delete;
  MojoFileWriter& operator=(const MojoFileWriter&) = delete;

  ~MojoFileWriter() override = default;

  // Requests |file_writer_| to open a new file for writing.
  void Open(const base::FilePath& filename, Writer::Callback callback);
  // Receives the result from Open() and returns it over the mojo channel.
  void OnFileOpened(BeginFileWriteCallback callback,
                    mojo::PendingAssociatedRemote<mojom::FileWriter> remote,
                    Writer::Result result);

  // mojom::FileWriter implementation.
  void WriteChunk(const std::vector<std::uint8_t>& data,
                  WriteChunkCallback callback) override;
  void CloseFile(CloseFileCallback callback) override;

  base::WeakPtr<MojoFileWriter> GetWeakPtr();

 private:
  const std::unique_ptr<Writer> file_writer_;
  base::WeakPtrFactory<MojoFileWriter> weak_ptr_factory{this};
};

MojoFileReader::MojoFileReader(std::unique_ptr<Reader> file_reader)
    : file_reader_(std::move(file_reader)) {}

base::WeakPtr<MojoFileReader> MojoFileReader::GetWeakPtr() {
  return weak_ptr_factory.GetWeakPtr();
}

void MojoFileReader::Open(Reader::OpenCallback callback) {
  file_reader_->Open(std::move(callback));
}

void MojoFileReader::OnFileOpened(
    BeginFileReadCallback callback,
    mojo::PendingAssociatedRemote<mojom::FileReader> remote,
    Reader::OpenResult result) {
  if (result.is_error()) {
    std::move(callback).Run(
        mojom::BeginFileReadResult::NewError(result.error()));
    // This will asynchronously trigger the destruction of this object.
    remote.reset();
    return;
  }

  std::move(callback).Run(mojom::BeginFileReadResult::NewSuccess(
      mojom::BeginFileReadSuccess::New(std::move(remote), filename(), size())));
}

// The Mojo-generated ReadChunkCallback has a const& param whereas the
// FileReader::ReadCallback does not so this function wraps the Mojo callback
// so it is compatible with the FileReader interface.
void ReadChunkCallbackWrapper(
    mojom::FileReader::ReadChunkCallback callback,
    protocol::FileTransferResult<std::vector<std::uint8_t>> result) {
  std::move(callback).Run(result);
}

void MojoFileReader::ReadChunk(uint64_t bytes_to_read,
                               ReadChunkCallback callback) {
  file_reader_->ReadChunk(
      bytes_to_read,
      base::BindOnce(&ReadChunkCallbackWrapper, std::move(callback)));
}

MojoFileWriter::MojoFileWriter(std::unique_ptr<Writer> file_writer)
    : file_writer_(std::move(file_writer)) {}

void MojoFileWriter::Open(const base::FilePath& filename,
                          Writer::Callback callback) {
  file_writer_->Open(filename, std::move(callback));
}

void MojoFileWriter::OnFileOpened(
    BeginFileWriteCallback callback,
    mojo::PendingAssociatedRemote<mojom::FileWriter> remote,
    Writer::Result result) {
  if (result.is_error()) {
    std::move(callback).Run(
        mojom::BeginFileWriteResult::NewError(result.error()));
    // This will asynchronously trigger the destruction of this object.
    remote.reset();
    return;
  }

  std::move(callback).Run(mojom::BeginFileWriteResult::NewSuccess(
      mojom::BeginFileWriteSuccess::New(std::move(remote))));
}

base::WeakPtr<MojoFileWriter> MojoFileWriter::GetWeakPtr() {
  return weak_ptr_factory.GetWeakPtr();
}

// Wraps a Mojo-generated callback to provide compatibility with the FileWriter
// methods which expect a Writer::Callback.
template <class T>
void FileWriterCallbackWrapper(T callback, remoting::Writer::Result result) {
  std::optional<protocol::FileTransfer_Error> error;
  if (result.is_error()) {
    error = std::move(result.error());
  }
  std::move(callback).Run(std::move(error));
}

void MojoFileWriter::WriteChunk(const std::vector<std::uint8_t>& data,
                                WriteChunkCallback callback) {
  file_writer_->WriteChunk(
      std::move(data),
      base::BindOnce(&FileWriterCallbackWrapper<WriteChunkCallback>,
                     std::move(callback)));
}

void MojoFileWriter::CloseFile(CloseFileCallback callback) {
  file_writer_->Close(base::BindOnce(
      &FileWriterCallbackWrapper<CloseFileCallback>, std::move(callback)));
}

}  // namespace

SessionFileOperationsHandler::SessionFileOperationsHandler(
    std::unique_ptr<FileOperations> file_operations)
    : file_operations_(std::move(file_operations)) {}

SessionFileOperationsHandler::~SessionFileOperationsHandler() = default;

void SessionFileOperationsHandler::BeginFileRead(
    BeginFileReadCallback callback) {
  mojo::AssociatedRemote<mojom::FileReader> remote;
  auto mojo_file_reader =
      std::make_unique<MojoFileReader>(file_operations_->CreateReader());
  MojoFileReader* ptr = mojo_file_reader.get();

  // |file_readers_| now manages the lifetime of |mojo_file_reader| and will
  // destroy the instance when |remote| is reset.
  file_readers_.Add(std::move(mojo_file_reader),
                    remote.BindNewEndpointAndPassReceiver());

  // We Unbind() |remote| so we can return it to the other end of the IPC
  // channel, this will not cause |mojo_file_reader| to be destroyed.
  ptr->Open(base::BindOnce(&MojoFileReader::OnFileOpened, ptr->GetWeakPtr(),
                           std::move(callback), remote.Unbind()));
}

void SessionFileOperationsHandler::BeginFileWrite(
    const base::FilePath& file_path,
    BeginFileWriteCallback callback) {
  mojo::AssociatedRemote<mojom::FileWriter> remote;
  auto mojo_file_writer =
      std::make_unique<MojoFileWriter>(file_operations_->CreateWriter());
  MojoFileWriter* ptr = mojo_file_writer.get();

  // |file_writers_| now manages the lifetime of |mojo_file_writer| and will
  // destroy the instance when |remote| is reset.
  file_writers_.Add(std::move(mojo_file_writer),
                    remote.BindNewEndpointAndPassReceiver());

  // We Unbind() |remote| so we can return it to the other end of the IPC
  // channel, this will not cause |mojo_file_writer| to be destroyed.
  ptr->Open(file_path,
            base::BindOnce(&MojoFileWriter::OnFileOpened, ptr->GetWeakPtr(),
                           std::move(callback), remote.Unbind()));
}

}  // namespace remoting
