/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/typed_arrays/array_buffer_builder.h"

#include <limits>
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace WTF {

static const int kDefaultBufferCapacity = 32768;

ArrayBufferBuilder::ArrayBufferBuilder()
    : bytes_used_(0), variable_capacity_(true) {
  buffer_ = ArrayBuffer::Create(kDefaultBufferCapacity, 1);
}

bool ArrayBufferBuilder::ExpandCapacity(unsigned size_to_increase) {
  size_t current_buffer_size = buffer_->ByteLength();

  // If the size of the buffer exceeds max of unsigned, it can't be grown any
  // more.
  if (size_to_increase > std::numeric_limits<unsigned>::max() - bytes_used_)
    return false;

  unsigned new_buffer_size = bytes_used_ + size_to_increase;

  // Grow exponentially if possible.
  unsigned exponential_growth_new_buffer_size =
      std::numeric_limits<unsigned>::max();
  if (current_buffer_size <= std::numeric_limits<unsigned>::max() / 2) {
    exponential_growth_new_buffer_size =
        static_cast<unsigned>(current_buffer_size * 2);
  }
  if (exponential_growth_new_buffer_size > new_buffer_size)
    new_buffer_size = exponential_growth_new_buffer_size;

  // Copy existing data in current buffer to new buffer.
  scoped_refptr<ArrayBuffer> new_buffer =
      ArrayBuffer::Create(new_buffer_size, 1);
  if (!new_buffer)
    return false;

  memcpy(new_buffer->Data(), buffer_->Data(), bytes_used_);
  buffer_ = new_buffer;
  return true;
}

unsigned ArrayBufferBuilder::Append(const char* data, unsigned length) {
  DCHECK_GT(length, 0u);

  size_t current_buffer_size = buffer_->ByteLength();

  DCHECK_LE(bytes_used_, current_buffer_size);

  size_t remaining_buffer_space = current_buffer_size - bytes_used_;

  unsigned bytes_to_save = length;

  if (length > remaining_buffer_space) {
    if (variable_capacity_) {
      if (!ExpandCapacity(length))
        return 0;
    } else {
      bytes_to_save = static_cast<unsigned>(remaining_buffer_space);
    }
  }

  memcpy(static_cast<char*>(buffer_->Data()) + bytes_used_, data,
         bytes_to_save);
  bytes_used_ += bytes_to_save;

  return bytes_to_save;
}

scoped_refptr<ArrayBuffer> ArrayBufferBuilder::ToArrayBuffer() {
  // Fully used. Return m_buffer as-is.
  if (buffer_->ByteLength() == bytes_used_)
    return buffer_;

  return buffer_->Slice(0, bytes_used_);
}

String ArrayBufferBuilder::ToString() {
  return String(static_cast<const char*>(buffer_->Data()), bytes_used_);
}

void ArrayBufferBuilder::ShrinkToFit() {
  DCHECK_LE(bytes_used_, buffer_->ByteLength());

  if (buffer_->ByteLength() > bytes_used_)
    buffer_ = buffer_->Slice(0, bytes_used_);
}

}  // namespace WTF
