// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_pipe_read_write_util.h"

#include "mojo/public/c/system/types.h"

namespace media {
namespace mojo_pipe_read_write_util {

bool IsPipeReadWriteError(MojoResult result) {
  return result != MOJO_RESULT_OK && result != MOJO_RESULT_SHOULD_WAIT;
}

}  // namespace mojo_pipe_read_write_util
}  // namespace media
