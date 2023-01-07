// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_UTILS_H_
#define MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_UTILS_H_

#include <stdint.h>

#include <string>

#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// Copies the data from |source| into |contents| and returns true on success and
// false on error.  In case of I/O error, |contents| holds the data that could
// be read from source before the error occurred.
bool MOJO_CPP_SYSTEM_EXPORT
BlockingCopyToString(ScopedDataPipeConsumerHandle source,
                     std::string* contents);

bool MOJO_CPP_SYSTEM_EXPORT
BlockingCopyFromString(const std::string& source,
                       const ScopedDataPipeProducerHandle& destination);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_UTILS_H_
