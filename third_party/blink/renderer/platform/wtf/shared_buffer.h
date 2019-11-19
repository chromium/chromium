/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SHARED_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SHARED_BUFFER_H_

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

class WTF_EXPORT SharedBuffer : public RefCounted<SharedBuffer> {
 public:
  // Iterator for ShreadBuffer contents. An Iterator will get invalid once the
  // associated SharedBuffer is modified (e.g., Append() is called). An Iterator
  // doesn't retain the associated container.
  class WTF_EXPORT Iterator final {
   public:
    ~Iterator() = default;

    Iterator& operator++();
    Iterator operator++(int) {
      auto temp = *this;
      ++*this;
      return temp;
    }
    bool operator==(const Iterator& that) const {
      return std::equal(value_.begin(), value_.end(), that.value_.begin(),
                        that.value_.end()) &&
             buffer_ == that.buffer_;
    }
    bool operator!=(const Iterator& that) const { return !(*this == that); }
    const base::span<const char>& operator*() const {
      DCHECK(!IsEnd());
      return value_;
    }
    const base::span<const char>* operator->() const {
      DCHECK(!IsEnd());
      return &value_;
    }

   private:
    friend class SharedBuffer;
    // for end()
    Iterator(const SharedBuffer* buffer);
    // for the consecutive part
    Iterator(size_t offset, const SharedBuffer* buffer);
    // for the rest
    Iterator(wtf_size_t segment_index,
             size_t offset,
             const SharedBuffer* buffer);

    void Init(size_t offset);
    bool IsEnd() const { return index_ == buffer_->segments_.size() + 1; }

    // It represents |buffer_->buffer| if |index_| is 0, and
    // |buffer_->segments[index_ - 1]| otherwise.
    wtf_size_t index_;
    base::span<const char> value_;
    const SharedBuffer* buffer_;
  };

  static constexpr unsigned kSegmentSize = 0x1000;

  static scoped_refptr<SharedBuffer> Create() {
    return base::AdoptRef(new SharedBuffer);
  }

  HAS_STRICTLY_TYPED_ARG
  static scoped_refptr<SharedBuffer> Create(STRICTLY_TYPED_ARG(size)) {
    ALLOW_NUMERIC_ARG_TYPES_PROMOTABLE_TO(size_t);
    return base::AdoptRef(new SharedBuffer(SafeCast<wtf_size_t>(size)));
  }

  HAS_STRICTLY_TYPED_ARG
  static scoped_refptr<SharedBuffer> Create(const char* data,
                                            STRICTLY_TYPED_ARG(size)) {
    ALLOW_NUMERIC_ARG_TYPES_PROMOTABLE_TO(size_t);
    return base::AdoptRef(new SharedBuffer(data, SafeCast<wtf_size_t>(size)));
  }

  HAS_STRICTLY_TYPED_ARG
  static scoped_refptr<SharedBuffer> Create(const unsigned char* data,
                                            STRICTLY_TYPED_ARG(size)) {
    ALLOW_NUMERIC_ARG_TYPES_PROMOTABLE_TO(size_t);
    return base::AdoptRef(new SharedBuffer(data, SafeCast<wtf_size_t>(size)));
  }

  static scoped_refptr<SharedBuffer> AdoptVector(Vector<char>&);

  ~SharedBuffer();

  // DEPRECATED: use a segment iterator, FlatData or explicit Copy() instead.
  //
  // Calling this function will force internal segmented buffers to be merged
  // into a flat buffer. Use iterator whenever possible for better performance.
  const char* Data();

  size_t size() const { return size_; }

  bool IsEmpty() const { return !size(); }

  void Append(const SharedBuffer&);

  HAS_STRICTLY_TYPED_ARG
  void Append(const char* data, STRICTLY_TYPED_ARG(size)) {
    ALLOW_NUMERIC_ARG_TYPES_PROMOTABLE_TO(size_t);
    AppendInternal(data, size);
  }
  void Append(const Vector<char>& data) { Append(data.data(), data.size()); }

  void Clear();

  Iterator begin() const;
  Iterator cbegin() const { return begin(); }
  Iterator end() const;
  Iterator cend() const { return end(); }

  // Copies the segmented data into a contiguous buffer.  Use GetSomeData() or
  // iterators if a copy is not required, as they are cheaper.
  // Supported Ts: WTF::Vector<char>, std::vector<char>.
  template <typename T>
  T CopyAs() const;

  // Returns an iterator for the given position of bytes. Returns |cend()| if
  // |position| is greater than or equal to |size()|.
  HAS_STRICTLY_TYPED_ARG
  Iterator GetIteratorAt(STRICTLY_TYPED_ARG(position)) const {
    STRICT_ARG_TYPE(size_t);
    return GetIteratorAtInternal(position);
  }

  // Copies |byteLength| bytes from the beginning of the content data into
  // |dest| as a flat buffer. Returns true on success, otherwise the content of
  // |dest| is not guaranteed.
  HAS_STRICTLY_TYPED_ARG
  WARN_UNUSED_RESULT
  bool GetBytes(void* dest, STRICTLY_TYPED_ARG(byte_length)) const {
    ALLOW_NUMERIC_ARG_TYPES_PROMOTABLE_TO(size_t);
    return GetBytesInternal(dest, byte_length);
  }

  void GetMemoryDumpNameAndSize(String& dump_name, size_t& dump_size) const;

  // Helper for providing a contiguous view of the data.  If the SharedBuffer is
  // segmented, this will copy/merge all segments into a temporary buffer.
  // In general, clients should use the efficient/segmented accessors.
  class WTF_EXPORT DeprecatedFlatData {
    STACK_ALLOCATED();

   public:
    explicit DeprecatedFlatData(scoped_refptr<const SharedBuffer>);

    const char* Data() const { return data_; }
    size_t size() const { return buffer_->size(); }

   private:
    scoped_refptr<const SharedBuffer> buffer_;
    Vector<char> flat_buffer_;
    const char* data_;
  };

 private:
  struct SegmentDeleter;
  using Segment = std::unique_ptr<char[], SegmentDeleter>;
  static Segment CreateSegment();

  SharedBuffer();
  explicit SharedBuffer(wtf_size_t);
  SharedBuffer(const char*, wtf_size_t);
  SharedBuffer(const unsigned char*, wtf_size_t);

  // See SharedBuffer::data().
  void MergeSegmentsIntoBuffer();

  void AppendInternal(const char* data, size_t);
  bool GetBytesInternal(void* dest, size_t) const;
  Iterator GetIteratorAtInternal(size_t position) const;
  size_t GetLastSegmentSize() const {
    DCHECK(!segments_.IsEmpty());
    return (size_ - buffer_.size() + kSegmentSize - 1) % kSegmentSize + 1;
  }

  size_t size_;
  Vector<char> buffer_;
  Vector<Segment> segments_;
};

// Current CopyAs specializations.
template <>
inline Vector<char> SharedBuffer::CopyAs() const {
  Vector<char> buffer;
  buffer.ReserveInitialCapacity(SafeCast<wtf_size_t>(size_));

  for (const auto& span : *this)
    buffer.Append(span.data(), static_cast<wtf_size_t>(span.size()));

  DCHECK_EQ(buffer.size(), size_);
  return buffer;
}

template <>
inline Vector<uint8_t> SharedBuffer::CopyAs() const {
  Vector<uint8_t> buffer;
  buffer.ReserveInitialCapacity(SafeCast<wtf_size_t>(size_));

  for (const auto& span : *this) {
    buffer.Append(reinterpret_cast<const uint8_t*>(span.data()),
                  static_cast<wtf_size_t>(span.size()));
  }

  DCHECK_EQ(buffer.size(), size_);
  return buffer;
}

template <>
inline std::vector<char> SharedBuffer::CopyAs() const {
  std::vector<char> buffer;
  buffer.reserve(size_);

  for (const auto& span : *this)
    buffer.insert(buffer.end(), span.data(), span.data() + span.size());

  DCHECK_EQ(buffer.size(), size_);
  return buffer;
}

template <>
inline std::vector<uint8_t> SharedBuffer::CopyAs() const {
  std::vector<uint8_t> buffer;
  buffer.reserve(size_);

  for (const auto& span : *this) {
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(span.data()),
                  reinterpret_cast<const uint8_t*>(span.data() + span.size()));
  }
  DCHECK_EQ(buffer.size(), size_);
  return buffer;
}

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_SHARED_BUFFER_H_
