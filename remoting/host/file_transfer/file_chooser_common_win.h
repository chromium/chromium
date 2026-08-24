// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_COMMON_WIN_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_COMMON_WIN_H_

#include <cstddef>
#include <cstdint>

#include "base/containers/span.h"
#include "remoting/host/file_transfer/file_chooser.h"

namespace remoting {

// This is the buffer size requested when calling CreatePipe, and is also the
// maximum amount of data the file chooser process will write to it (so the
// correct error can be generated in that case). The CreatePipe documentation
// notes that the size specified is only advisory, but 4096 is small enough to
// be safe in any event, and is more than big enough to return any path we're
// likely to see on Windows.
constexpr std::size_t kFileChooserPipeBufferSize = 4096;

// Parses and deserializes the response bytes received from the file chooser
// child process. Validates the Mojo message header to ensure safe handling
// across privilege boundaries.
FileChooser::Result ParseFileChooserResponse(
    base::span<const uint8_t> response_bytes);

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_COMMON_WIN_H_
