// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/rtc_log_file_operations.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "remoting/protocol/webrtc_event_log_data.h"

namespace remoting {

namespace {

class RtcLogFileReader : public FileOperations::Reader {
 public:
  explicit RtcLogFileReader(protocol::ConnectionToClient* connection);
  ~RtcLogFileReader() override;
  RtcLogFileReader(const RtcLogFileReader&) = delete;
  RtcLogFileReader& operator=(const RtcLogFileReader&) = delete;

  // FileOperations::Reader interface.
  void Open(OpenCallback callback) override;
  void ReadChunk(std::size_t size, ReadCallback callback) override;
  const base::FilePath& filename() const override;
  std::uint64_t size() const override;
  FileOperations::State state() const override;

 private:
  using LogSection = protocol::WebrtcEventLogData::LogSection;

  void DoOpen(OpenCallback callback);
  void DoReadChunk(std::size_t size, ReadCallback callback);

  // Reads up to |maximum_to_read| bytes from the event log, and appends them
  // to |output| and returns the number of bytes appended. This only reads from
  // a single LogSection, and it takes care of advancing to the next LogSection
  // if the end is reached. Returns 0 if there is no more data to be read.
  int ReadPartially(int maximum_to_read, std::vector<std::uint8_t>& output);

  raw_ptr<protocol::ConnectionToClient> connection_;
  base::FilePath filename_;
  base::circular_deque<LogSection> data_;
  FileOperations::State state_ = FileOperations::kCreated;

  // Points to the current LogSection being read from, or data_.end() if
  // reading is finished.
  base::circular_deque<LogSection>::const_iterator current_log_section_;

  // Points to the current read position inside |current_log_section_| or is
  // undefined if current_log_section_ == data_.end(). Note that each
  // LogSection of |data_| is always non-empty. If the end of a LogSection is
  // reached, |current_log_section_| will advance to the next section, and this
  // position will be reset to the beginning of the new section.
  LogSection::const_iterator current_position_;

  base::WeakPtrFactory<RtcLogFileReader> weak_factory_{this};
};

// This class simply returns a protocol error if the client attempts to upload
// a file to this FileOperations implementation. The RTC log is download-only,
// and the upload code-path is never intended to be executed. This class is
// intended to gracefully return an error instead of crashing the host process.
class RtcLogFileWriter : public FileOperations::Writer {
 public:
  RtcLogFileWriter() = default;
  ~RtcLogFileWriter() override = default;
  RtcLogFileWriter(const RtcLogFileWriter&) = delete;
  RtcLogFileWriter& operator=(const RtcLogFileWriter&) = delete;

  // FileOperations::Writer interface.
  void Open(const base::FilePath& filename, Callback callback) override;
  void WriteChunk(std::vector<std::uint8_t> data, Callback callback) override;
  void Close(Callback callback) override;
  FileOperations::State state() const override;
};

RtcLogFileReader::RtcLogFileReader(protocol::ConnectionToClient* connection)
    : connection_(connection) {}
RtcLogFileReader::~RtcLogFileReader() = default;

void RtcLogFileReader::Open(OpenCallback callback) {
  state_ = FileOperations::kBusy;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RtcLogFileReader::DoOpen, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void RtcLogFileReader::ReadChunk(std::size_t size, ReadCallback callback) {
  state_ = FileOperations::kBusy;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RtcLogFileReader::DoReadChunk, weak_factory_.GetWeakPtr(),
                     size, std::move(callback)));
}

const base::FilePath& RtcLogFileReader::filename() const {
  return filename_;
}

std::uint64_t RtcLogFileReader::size() const {
  std::uint64_t result = 0;
  for (const auto& section : data_) {
    result += section.size();
  }
  return result;
}

FileOperations::State RtcLogFileReader::state() const {
  return state_;
}

void RtcLogFileReader::DoOpen(OpenCallback callback) {
  protocol::WebrtcEventLogData* rtc_log = connection_->rtc_event_log();
  if (!rtc_log) {
    // This is a protocol error because RTC log is only supported for WebRTC
    // connections.
    state_ = FileOperations::kFailed;
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
    return;
  }

  filename_ =
      base::FilePath::FromUTF8Unsafe(base::UnlocalizedTimeFormatWithPattern(
          base::Time::NowFromSystemTime(), "'host-rtc-log'-y-M-d_H-m-s"));

  data_ = rtc_log->TakeLogData();
  current_log_section_ = data_.begin();
  if (!data_.empty()) {
    current_position_ = (*current_log_section_).begin();
  }

  state_ = FileOperations::kReady;
  std::move(callback).Run(kSuccessTag);
}

void RtcLogFileReader::DoReadChunk(std::size_t size, ReadCallback callback) {
  std::vector<std::uint8_t> result;
  int bytes_read;
  int bytes_remaining = static_cast<int>(size);
  while (bytes_remaining &&
         (bytes_read = ReadPartially(bytes_remaining, result)) > 0) {
    bytes_remaining -= bytes_read;
  }

  state_ = result.empty() ? FileOperations::kComplete : FileOperations::kReady;
  std::move(callback).Run(result);
}

int RtcLogFileReader::ReadPartially(int maximum_to_read,
                                    std::vector<std::uint8_t>& output) {
  if (data_.empty()) {
    return 0;
  }

  if (current_log_section_ == data_.end()) {
    return 0;
  }

  const auto& section = *current_log_section_;
  DCHECK(section.begin() <= current_position_);
  DCHECK(current_position_ < section.end());

  int remaining_in_section = section.end() - current_position_;
  int read_amount = std::min(remaining_in_section, maximum_to_read);

  output.insert(output.end(), current_position_,
                current_position_ + read_amount);
  current_position_ += read_amount;

  if (current_position_ == section.end()) {
    // Advance to beginning of next LogSection.
    current_log_section_++;
    if (current_log_section_ != data_.end()) {
      current_position_ = (*current_log_section_).begin();
    }
  }

  return read_amount;
}

void RtcLogFileWriter::Open(const base::FilePath& filename, Callback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          protocol::MakeFileTransferError(
              FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR)));
}

void RtcLogFileWriter::WriteChunk(std::vector<std::uint8_t> data,
                                  Callback callback) {
  NOTREACHED();
}

void RtcLogFileWriter::Close(Callback callback) {
  NOTREACHED();
}

FileOperations::State RtcLogFileWriter::state() const {
  return FileOperations::State::kFailed;
}

}  // namespace

RtcLogFileOperations::RtcLogFileOperations(
    protocol::ConnectionToClient* connection)
    : connection_(connection) {}

RtcLogFileOperations::~RtcLogFileOperations() = default;

std::unique_ptr<FileOperations::Reader> RtcLogFileOperations::CreateReader() {
  return std::make_unique<RtcLogFileReader>(connection_);
}

std::unique_ptr<FileOperations::Writer> RtcLogFileOperations::CreateWriter() {
  return std::make_unique<RtcLogFileWriter>();
}

}  // namespace remoting
