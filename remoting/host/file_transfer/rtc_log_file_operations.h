// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_RTC_LOG_FILE_OPERATIONS_H_
#define REMOTING_HOST_FILE_TRANSFER_RTC_LOG_FILE_OPERATIONS_H_

#include "base/memory/raw_ptr.h"
#include "remoting/host/file_transfer/file_operations.h"

namespace remoting {

namespace protocol {
class ConnectionToClient;
}  // namespace protocol

// Implementation of FileOperations that sends the RTC event log to the client.
// As the event log is held in memory, there is no need for blocking IO and
// this class can run on the network thread.
class RtcLogFileOperations : public FileOperations {
 public:
  explicit RtcLogFileOperations(protocol::ConnectionToClient* connection);
  ~RtcLogFileOperations() override;
  RtcLogFileOperations(const RtcLogFileOperations&) = delete;
  RtcLogFileOperations& operator=(const RtcLogFileOperations&) = delete;

  // FileOperations interface.
  std::unique_ptr<Reader> CreateReader() override;
  std::unique_ptr<Writer> CreateWriter() override;

 private:
  raw_ptr<protocol::ConnectionToClient> connection_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_RTC_LOG_FILE_OPERATIONS_H_
