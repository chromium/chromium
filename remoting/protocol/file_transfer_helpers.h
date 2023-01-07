// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FILE_TRANSFER_HELPERS_H_
#define REMOTING_PROTOCOL_FILE_TRANSFER_HELPERS_H_

#include <cstdint>
#include <ostream>

#include "base/location.h"
#include "remoting/base/result.h"
#include "remoting/proto/file_transfer.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remoting::protocol {

template <typename SuccessType>
using FileTransferResult = Result<SuccessType, FileTransfer_Error>;

FileTransfer_Error MakeFileTransferError(
    base::Location location,
    FileTransfer_Error_Type type,
    absl::optional<std::int32_t> api_error_code = absl::nullopt);

std::ostream& operator<<(std::ostream& stream, const FileTransfer_Error& error);

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FILE_TRANSFER_HELPERS_H_
