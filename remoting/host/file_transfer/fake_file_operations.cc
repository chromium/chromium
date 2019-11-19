// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/fake_file_operations.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace remoting {

class FakeFileOperations::FakeFileReader : public FileOperations::Reader {
 public:
  FakeFileReader(TestIo* test_io);
  ~FakeFileReader() override;

  void Open(OpenCallback callback) override;
  void ReadChunk(std::size_t size, ReadCallback callback) override;

  const base::FilePath& filename() const override;
  std::uint64_t size() const override;
  State state() const override;

 private:
  void DoOpen(OpenCallback callback);
  void DoReadChunk(std::size_t size, ReadCallback callback);

  FileOperations::State state_ = FileOperations::kCreated;
  TestIo* test_io_;
  protocol::FileTransferResult<InputFile> input_file_;
  base::FilePath filename_;
  std::size_t filesize_ = 0;
  std::size_t read_offset_ = 0;
  base::WeakPtrFactory<FakeFileReader> weak_ptr_factory_{this};
};

class FakeFileOperations::FakeFileWriter : public FileOperations::Writer {
 public:
  FakeFileWriter(TestIo* test_io);
  ~FakeFileWriter() override;

  void Open(const base::FilePath& filename, Callback callback) override;
  void WriteChunk(std::string data, Callback callback) override;
  void Close(Callback callback) override;
  FileOperations::State state() const override;

 private:
  void DoOpen(Callback callback);
  void DoWrite(std::string data, Callback callback);
  void DoClose(Callback callback);
  FileOperations::State state_ = FileOperations::kCreated;
  TestIo* test_io_;
  base::FilePath filename_;
  std::vector<std::string> chunks_;
  base::WeakPtrFactory<FakeFileWriter> weak_ptr_factory_{this};
};

std::unique_ptr<FileOperations::Reader> FakeFileOperations::CreateReader() {
  return std::make_unique<FakeFileReader>(test_io_);
}

std::unique_ptr<FileOperations::Writer> FakeFileOperations::CreateWriter() {
  return std::make_unique<FakeFileWriter>(test_io_);
}

FakeFileOperations::FakeFileOperations(FakeFileOperations::TestIo* test_io)
    : test_io_(test_io) {}

FakeFileOperations::~FakeFileOperations() = default;

FakeFileOperations::OutputFile::OutputFile(base::FilePath filename,
                                           bool failed,
                                           std::vector<std::string> chunks)
    : filename(std::move(filename)),
      failed(failed),
      chunks(std::move(chunks)) {}

FakeFileOperations::OutputFile::OutputFile(const OutputFile& other) = default;
FakeFileOperations::OutputFile::OutputFile(OutputFile&& other) = default;
FakeFileOperations::OutputFile& FakeFileOperations::OutputFile::operator=(
    const OutputFile&) = default;
FakeFileOperations::OutputFile& FakeFileOperations::OutputFile::operator=(
    OutputFile&&) = default;
FakeFileOperations::OutputFile::~OutputFile() = default;

FakeFileOperations::InputFile::InputFile(
    base::FilePath filename,
    std::string data,
    base::Optional<protocol::FileTransfer_Error> io_error)
    : filename(std::move(filename)),
      data(std::move(data)),
      io_error(std::move(io_error)) {}

FakeFileOperations::InputFile::InputFile() = default;
FakeFileOperations::InputFile::InputFile(const InputFile&) = default;
FakeFileOperations::InputFile::InputFile(InputFile&&) = default;
FakeFileOperations::InputFile& FakeFileOperations::InputFile::operator=(
    const InputFile&) = default;
FakeFileOperations::InputFile& FakeFileOperations::InputFile::operator=(
    InputFile&&) = default;
FakeFileOperations::InputFile::~InputFile() = default;

FakeFileOperations::TestIo::TestIo() = default;
FakeFileOperations::TestIo::TestIo(const TestIo& other) = default;
FakeFileOperations::TestIo::~TestIo() = default;

FakeFileOperations::FakeFileReader::FakeFileReader(TestIo* test_io)
    : test_io_(test_io) {}

FakeFileOperations::FakeFileReader::~FakeFileReader() = default;

void FakeFileOperations::FakeFileReader::Open(
    FileOperations::Reader::OpenCallback callback) {
  CHECK_EQ(kCreated, state_) << "Open called twice";
  state_ = kBusy;
  input_file_ = test_io_->input_file;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeFileReader::DoOpen, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void FakeFileOperations::FakeFileReader::ReadChunk(
    std::size_t size,
    FileOperations::Reader::ReadCallback callback) {
  CHECK_EQ(kReady, state_) << "ReadChunk called when writer not ready";
  state_ = kBusy;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeFileReader::DoReadChunk,
                                weak_ptr_factory_.GetWeakPtr(), size,
                                std::move(callback)));
}

const base::FilePath& FakeFileOperations::FakeFileReader::filename() const {
  return filename_;
}

std::uint64_t FakeFileOperations::FakeFileReader::size() const {
  return filesize_;
}

FileOperations::State FakeFileOperations::FakeFileReader::state() const {
  return state_;
}

void FakeFileOperations::FakeFileReader::DoOpen(
    FileOperations::Reader::OpenCallback callback) {
  if (input_file_) {
    filename_ = input_file_->filename;
    filesize_ = input_file_->data.size();
    state_ = kReady;
    std::move(callback).Run(kSuccessTag);
  } else {
    state_ = kFailed;
    std::move(callback).Run(input_file_.error());
  }
}

void FakeFileOperations::FakeFileReader::DoReadChunk(
    std::size_t size,
    FileOperations::Reader::ReadCallback callback) {
  if (size == 0) {
    state_ = kReady;
    std::move(callback).Run(std::string());
    return;
  }

  std::size_t remaining_data = input_file_->data.size() - read_offset_;

  if (remaining_data == 0) {
    if (input_file_->io_error) {
      state_ = kFailed;
      std::move(callback).Run(*input_file_->io_error);
    } else {
      state_ = kComplete;
      std::move(callback).Run(std::string());
    }
    return;
  }

  std::size_t read_size = std::min(size, remaining_data);
  state_ = kReady;
  std::move(callback).Run(
      std::string(input_file_->data, read_offset_, read_size));
  read_offset_ += read_size;
}

FakeFileOperations::FakeFileWriter::FakeFileWriter(TestIo* test_io)
    : test_io_(test_io) {}

FakeFileOperations::FakeFileWriter::~FakeFileWriter() {
  if (state_ == FileOperations::kCreated ||
      state_ == FileOperations::kComplete ||
      state_ == FileOperations::kFailed) {
    return;
  }

  test_io_->files_written.push_back(
      OutputFile(filename_, true /* failed */, std::move(chunks_)));
}

void FakeFileOperations::FakeFileWriter::Open(const base::FilePath& filename,
                                              Callback callback) {
  CHECK_EQ(kCreated, state_) << "Open called twice";
  state_ = kBusy;
  filename_ = filename;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeFileWriter::DoOpen, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void FakeFileOperations::FakeFileWriter::WriteChunk(std::string data,
                                                    Callback callback) {
  CHECK_EQ(kReady, state_) << "WriteChunk called when writer not ready";
  state_ = kBusy;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeFileWriter::DoWrite, weak_ptr_factory_.GetWeakPtr(),
                     std::move(data), std::move(callback)));
}

void FakeFileOperations::FakeFileWriter::Close(Callback callback) {
  CHECK_EQ(kReady, state_) << "Close called when writer not ready";
  state_ = kBusy;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeFileWriter::DoClose, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

FileOperations::State FakeFileOperations::FakeFileWriter::state() const {
  return state_;
}

void FakeFileOperations::FakeFileWriter::DoOpen(Callback callback) {
  if (!test_io_->io_error) {
    state_ = kReady;
    std::move(callback).Run(kSuccessTag);
  } else {
    state_ = kFailed;
    std::move(callback).Run(*test_io_->io_error);
  }
}

void FakeFileOperations::FakeFileWriter::DoWrite(std::string data,
                                                 Callback callback) {
  if (!test_io_->io_error) {
    chunks_.push_back(std::move(data));
    state_ = kReady;
    std::move(callback).Run(kSuccessTag);
  } else {
    state_ = kFailed;
    test_io_->files_written.push_back(
        OutputFile(filename_, true /* failed */, std::move(chunks_)));
    std::move(callback).Run(*test_io_->io_error);
  }
}

void FakeFileOperations::FakeFileWriter::DoClose(Callback callback) {
  if (!test_io_->io_error) {
    test_io_->files_written.push_back(
        OutputFile(filename_, false /* failed */, std::move(chunks_)));
    state_ = kComplete;
    std::move(callback).Run(kSuccessTag);
  } else {
    state_ = kFailed;
    test_io_->files_written.push_back(
        OutputFile(filename_, true /* failed */, std::move(chunks_)));
    std::move(callback).Run(*test_io_->io_error);
  }
}

}  // namespace remoting
