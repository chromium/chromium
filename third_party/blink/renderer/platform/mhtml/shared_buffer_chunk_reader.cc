/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/mhtml/shared_buffer_chunk_reader.h"

#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

SharedBufferChunkReader::SharedBufferChunkReader(
    scoped_refptr<const SharedBuffer> buffer,
    const Vector<char>& separator)
    : buffer_(std::move(buffer)),
      buffer_position_(0),
      segment_(nullptr),
      segment_length_(0),
      segment_index_(0),
      reached_end_of_file_(false),
      separator_(separator),
      separator_index_(0) {}

SharedBufferChunkReader::SharedBufferChunkReader(
    scoped_refptr<const SharedBuffer> buffer,
    const char* separator)
    : buffer_(std::move(buffer)),
      buffer_position_(0),
      segment_(nullptr),
      segment_length_(0),
      segment_index_(0),
      reached_end_of_file_(false),
      separator_index_(0) {
  SetSeparator(separator);
}

void SharedBufferChunkReader::SetSeparator(const Vector<char>& separator) {
  separator_ = separator;
}

void SharedBufferChunkReader::SetSeparator(const char* separator) {
  separator_.clear();
  separator_.Append(separator, SafeCast<wtf_size_t>(strlen(separator)));
}

bool SharedBufferChunkReader::NextChunk(Vector<char>& chunk,
                                        bool include_separator) {
  if (reached_end_of_file_)
    return false;

  chunk.clear();
  while (true) {
    while (segment_index_ < segment_length_) {
      char current_character = segment_[segment_index_++];
      if (current_character != separator_[separator_index_]) {
        if (separator_index_ > 0) {
          SECURITY_DCHECK(separator_index_ <= separator_.size());
          chunk.Append(separator_.data(), separator_index_);
          separator_index_ = 0;
        }
        chunk.push_back(current_character);
        continue;
      }
      separator_index_++;
      if (separator_index_ == separator_.size()) {
        if (include_separator)
          chunk.AppendVector(separator_);
        separator_index_ = 0;
        return true;
      }
    }

    // Read the next segment.
    segment_index_ = 0;
    buffer_position_ += segment_length_;
    auto it = buffer_->GetIteratorAt(buffer_position_);
    if (it == buffer_->cend()) {
      segment_ = nullptr;
      segment_length_ = 0;
      reached_end_of_file_ = true;
      if (separator_index_ > 0)
        chunk.Append(separator_.data(), separator_index_);
      return !chunk.IsEmpty();
    }
    segment_ = it->data();
    segment_length_ = SafeCast<uint32_t>(it->size());
  }
  NOTREACHED();
  return false;
}

String SharedBufferChunkReader::NextChunkAsUTF8StringWithLatin1Fallback(
    bool include_separator) {
  Vector<char> data;
  if (!NextChunk(data, include_separator))
    return String();

  return data.size()
             ? String::FromUTF8WithLatin1Fallback(data.data(), data.size())
             : g_empty_string;
}

uint32_t SharedBufferChunkReader::Peek(Vector<char>& data,
                                       uint32_t requested_size) {
  data.clear();
  if (requested_size <= segment_length_ - segment_index_) {
    data.Append(segment_ + segment_index_, requested_size);
    return requested_size;
  }

  uint32_t read_bytes_count = segment_length_ - segment_index_;
  data.Append(segment_ + segment_index_, read_bytes_count);

  for (auto it = buffer_->GetIteratorAt(buffer_position_ + segment_length_);
       it != buffer_->cend(); ++it) {
    if (requested_size <= read_bytes_count + it->size()) {
      data.Append(it->data(), requested_size - read_bytes_count);
      read_bytes_count += (requested_size - read_bytes_count);
      break;
    }
    data.Append(it->data(), SafeCast<wtf_size_t>(it->size()));
    read_bytes_count += it->size();
  }
  return read_bytes_count;
}

}  // namespace blink
