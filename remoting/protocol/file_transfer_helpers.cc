// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting::protocol {

FileTransfer_Error MakeFileTransferError(
    base::Location location,
    FileTransfer_Error_Type type,
    std::optional<int32_t> api_error_code) {
  FileTransfer_Error error;
  error.set_type(type);
  if (api_error_code) {
    error.set_api_error_code(*api_error_code);
  }
  error.set_function(location.function_name());
  error.set_source_file(location.file_name());
  error.set_line_number(location.line_number());
  return error;
}

std::ostream& operator<<(std::ostream& stream,
                         const FileTransfer_Error& error) {
  stream << "[" << error.source_file() << ":" << error.line_number() << " ("
         << error.function() << ")";
  if (error.has_api_error_code()) {
    stream << ": " << error.api_error_code();
  }
  stream << "]";
  return stream;
}

}  // namespace remoting::protocol
