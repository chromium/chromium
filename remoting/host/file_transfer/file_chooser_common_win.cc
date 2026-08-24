// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser_common_win.h"

#include <utility>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

FileChooser::Result ParseFileChooserResponse(
    base::span<const uint8_t> response_bytes) {
  // TODO(crbug.com/517336350): Replace raw pipe serialization with a standard
  // Mojo IPC channel to avoid manual deserialization across privilege
  // boundaries.
  mojo::Message serialized_message(response_bytes,
                                   base::span<mojo::ScopedHandle>());

  mojo::MessageHeaderValidator validator;
  if (!validator.Accept(&serialized_message)) {
    LOG(ERROR) << "Failed to validate message header from file chooser.";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }

  FileChooser::Result result;
  if (!mojom::FileChooserResult::DeserializeFromMessage(
          std::move(serialized_message), &result)) {
    LOG(ERROR) << "Failed to deserialize response.";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR);
  }

  return result;
}

}  // namespace remoting
