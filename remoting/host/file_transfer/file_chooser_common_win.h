// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_COMMON_WIN_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_COMMON_WIN_H_

#include <cstddef>

namespace remoting {

// This is the buffer size requested when calling CreatePipe, and is also the
// maximum amount of data the file chooser process will write to it (so the
// correct error can be generated in that case). The CreatePipe documentation
// notes that the size specified is only advisory, but 4096 is small enough to
// be safe in any event, and is more than big enough to return any path we're
// likely to see on Windows.
constexpr std::size_t kFileChooserPipeBufferSize = 4096;

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_CHOOSER_COMMON_WIN_H_
