// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_utils.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/system/wait.h"

namespace mojo {
namespace {

bool BlockingCopyHelper(
    ScopedDataPipeConsumerHandle source,
    base::RepeatingCallback<size_t(base::span<const uint8_t>)> write_bytes) {
  for (;;) {
    base::span<const uint8_t> buffer;
    MojoResult result = source->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (result == MOJO_RESULT_OK) {
      size_t bytes_written = write_bytes.Run(buffer);
      result = source->EndReadData(buffer.size());
      if (bytes_written < buffer.size() || result != MOJO_RESULT_OK) {
        return false;
      }
    } else if (result == MOJO_RESULT_SHOULD_WAIT) {
      result = Wait(source.get(), MOJO_HANDLE_SIGNAL_READABLE);
      if (result != MOJO_RESULT_OK) {
        // If the producer handle was closed, then treat as EOF.
        return result == MOJO_RESULT_FAILED_PRECONDITION;
      }
    } else if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      // If the producer handle was closed, then treat as EOF.
      return true;
    } else {
      // Some other error occurred.
      break;
    }
  }

  return false;
}

size_t CopyToStringHelper(std::string* result,
                          base::span<const uint8_t> buffer) {
  result->append(base::as_string_view(buffer));
  return buffer.size();
}

}  // namespace

// TODO(hansmuller): Add a max_size parameter.
bool BlockingCopyToString(ScopedDataPipeConsumerHandle source,
                          std::string* result) {
  CHECK(result);
  result->clear();
  return BlockingCopyHelper(std::move(source),
                            base::BindRepeating(&CopyToStringHelper, result));
}

bool MOJO_CPP_SYSTEM_EXPORT
BlockingCopyFromString(const std::string& source_str,
                       const ScopedDataPipeProducerHandle& destination) {
  base::span<const uint8_t> source = base::as_byte_span(source_str);
  while (!source.empty()) {
    base::span<uint8_t> dest;
    size_t size_hint = source.size();
    MojoResult result =
        destination->BeginWriteData(size_hint, MOJO_WRITE_DATA_FLAG_NONE, dest);
    if (result == MOJO_RESULT_OK) {
      size_t copy_size = std::min(source.size(), dest.size());
      dest.copy_prefix_from(source.first(copy_size));
      destination->EndWriteData(copy_size);
      source = source.subspan(copy_size);
    } else if (result == MOJO_RESULT_SHOULD_WAIT) {
      result = Wait(destination.get(), MOJO_HANDLE_SIGNAL_WRITABLE);
      if (result != MOJO_RESULT_OK) {
        // If the consumer handle was closed, then treat as EOF.
        return result == MOJO_RESULT_FAILED_PRECONDITION;
      }
    } else {
      // If the consumer handle was closed, then treat as EOF.
      return result == MOJO_RESULT_FAILED_PRECONDITION;
    }
  }
  return true;
}

}  // namespace mojo
