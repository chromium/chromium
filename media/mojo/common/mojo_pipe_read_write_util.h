// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_MOJO_PIPE_READ_WRITE_UTIL_H_
#define MEDIA_MOJO_COMMON_MOJO_PIPE_READ_WRITE_UTIL_H_

#include "mojo/public/c/system/types.h"

namespace media {
namespace mojo_pipe_read_write_util {

bool IsPipeReadWriteError(MojoResult result);

}  // namespace mojo_pipe_read_write_util
}  // namespace media

#endif  // MEDIA_MOJO_COMMON_MOJO_PIPE_READ_WRITE_UTIL_H_
