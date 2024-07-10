/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/xml/parser/shared_buffer_reader.h"

#include <algorithm>
#include <cstring>

#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

SharedBufferReader::SharedBufferReader(scoped_refptr<const SharedBuffer> buffer)
    : buffer_(std::move(buffer)), current_offset_(0) {}

SharedBufferReader::~SharedBufferReader() = default;

int SharedBufferReader::ReadData(char* output_buffer, int asked_to_read) {
  if (!buffer_ || current_offset_ > buffer_->size())
    return 0;

  size_t bytes_copied = 0;
  size_t len_to_copy = std::min(base::checked_cast<size_t>(asked_to_read),
                                buffer_->size() - current_offset_);
  for (auto it = buffer_->GetIteratorAt(current_offset_); it != buffer_->cend();
       ++it) {
    if (bytes_copied >= len_to_copy)
      break;
    size_t to_be_written = std::min(it->size(), len_to_copy - bytes_copied);

    memcpy(output_buffer + bytes_copied, it->data(), to_be_written);
    bytes_copied += to_be_written;
  }

  current_offset_ += bytes_copied;
  return base::checked_cast<int>(bytes_copied);
}

}  // namespace blink
