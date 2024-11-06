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

size_t SharedBufferReader::ReadData(base::span<char> output_buffer) {
  if (!buffer_ || current_offset_ > buffer_->size())
    return 0;

  const size_t output_buffer_size = output_buffer.size();
  for (auto it = buffer_->GetIteratorAt(current_offset_); it != buffer_->cend();
       ++it) {
    const size_t to_be_written = std::min(it->size(), output_buffer.size());
    output_buffer.copy_prefix_from(it->first(to_be_written));
    output_buffer = output_buffer.subspan(to_be_written);
    if (output_buffer.empty()) {
      break;
    }
  }

  const size_t bytes_copied = output_buffer_size - output_buffer.size();
  current_offset_ += bytes_copied;
  return bytes_copied;
}

}  // namespace blink
