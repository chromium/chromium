// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Streams classes.
//
// These memory-resident streams are used for serializing data into a sequential
// region of memory.
// Streams are divided into SourceStreams for reading and SinkStreams for
// writing.  Streams are aggregated into Sets which allows several streams to be
// used at once.  Example: we can write A1, B1, A2, B2 but achieve the memory
// layout A1 A2 B1 B2 by writing 'A's to one stream and 'B's to another.

#ifndef COURGETTE_STREAMS_H_
#define COURGETTE_STREAMS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>  // for FILE*
#include <string>

#include "courgette/memory_allocator.h"
#include "courgette/region.h"


namespace courgette {

class SourceStream;
class SinkStream;

// Maximum number of streams in a stream set.
static const unsigned int kMaxStreams = 10;

// A simple interface for reading binary data.
class BasicBuffer {
 public:
  BasicBuffer() {}
  virtual ~BasicBuffer() {}
  virtual const uint8_t* data() const = 0;
  virtual size_t length() const = 0;
};

// A SourceStream allows a region of memory to be scanned by a sequence of Read
// operations.  The stream does not own the memory.
class SourceStream {
 public:
  SourceStream() : start_(nullptr), end_(nullptr), current_(nullptr) {}

  SourceStream(const SourceStream&) = delete;
  SourceStream& operator=(const SourceStream&) = delete;

  // Initializes the SourceStream to yield the bytes at |pointer|.  The caller
  // still owns the memory at |pointer| and should free the memory only after
  // the last use of the stream.
  void Init(const void* pointer, size_t length) {
    start_ = static_cast<const uint8_t*>(pointer);
    end_ = start_ + length;
    current_ = start_;
  }

  // Initializes the SourceStream to yield the bytes in |region|.  The caller
  // still owns the memory at |region| and should free the memory only after
  // the last use of the stream.
  void Init(const Region& region) { Init(region.start(), region.length()); }

  // Initializes the SourceStream to yield the bytes in |string|.  The caller
  // still owns the memory at |string| and should free the memory only after
  // the last use of the stream.
  void Init(const std::string& string) { Init(string.c_str(), string.size()); }

  // Initializes the SourceStream to yield the bytes written to |sink|. |sink|
  // still owns the memory, so needs to outlive |this|.  |sink| should not be
  // written to after |this| is initialized.
  void Init(const SinkStream& sink);

  // Returns number of bytes remaining to be read from stream.
  size_t Remaining() const { return end_ - current_; }

  // Returns initial length of stream before any data consumed by reading.
  size_t OriginalLength() const { return end_ - start_; }

  const uint8_t* Buffer() const { return current_; }
  bool Empty() const { return current_ == end_; }

  // Copies bytes from stream to memory at |destination|.  Returns 'false' if
  // insufficient data to satisfy request.
  bool Read(void* destination, size_t byte_count);

  // Reads a varint formatted unsigned integer from stream.  Returns 'false' if
  // the read failed due to insufficient data or malformed Varint32.
  bool ReadVarint32(uint32_t* output_value);

  // Reads a varint formatted signed integer from stream.  Returns 'false' if
  // the read failed due to insufficient data or malformed Varint32.
  bool ReadVarint32Signed(int32_t* output_value);

  // Initializes |substream| to yield |length| bytes from |this| stream,
  // starting at |offset| bytes from the current position.  Returns 'false' if
  // there are insufficient bytes in |this| stream.
  bool ShareSubstream(size_t offset, size_t length, SourceStream* substream);

  // Initializes |substream| to yield |length| bytes from |this| stream,
  // starting at the current position.  Returns 'false' if there are
  // insufficient bytes in |this| stream.
  bool ShareSubstream(size_t length, SourceStream* substream) {
    return ShareSubstream(0, length, substream);
  }

  // Reads |length| bytes from |this| stream.  Initializes |substream| to yield
  // the bytes.  Returns 'false' if there are insufficient bytes in |this|
  // stream.
  bool ReadSubstream(size_t length, SourceStream* substream);

  // Skips over bytes.  Returns 'false' if insufficient data to satisfy request.
  bool Skip(size_t byte_count);

 private:
  const uint8_t* start_;    // Points to start of buffer.
  const uint8_t* end_;      // Points to first location after buffer.
  const uint8_t* current_;  // Points into buffer at current read location.
};

// A SinkStream accumulates writes into a buffer that it owns.  The stream is
// initially in an 'accumulating' state where writes are permitted.  Accessing
// the buffer moves the stream into a 'locked' state where no more writes are
// permitted.  The stream may also be in a 'retired' state where the buffer
// contents are no longer available.
class SinkStream {
 public:
  SinkStream() {}

  SinkStream(const SinkStream&) = delete;
  SinkStream& operator=(const SinkStream&) = delete;

  ~SinkStream() {}

  // Appends |byte_count| bytes from |data| to the stream.
  [[nodiscard]] CheckBool Write(const void* data, size_t byte_count);

  // Appends the 'varint32' encoding of |value| to the stream.
  [[nodiscard]] CheckBool WriteVarint32(uint32_t value);

  // Appends the 'varint32' encoding of |value| to the stream.
  [[nodiscard]] CheckBool WriteVarint32Signed(int32_t value);

  // Appends the 'varint32' encoding of |value| to the stream.
  // On platforms where sizeof(size_t) != sizeof(int32_t), do a safety check.
  [[nodiscard]] CheckBool WriteSizeVarint32(size_t value);

  // Contents of |other| are appended to |this| stream.  The |other| stream
  // becomes retired.
  [[nodiscard]] CheckBool Append(SinkStream* other);

  // Returns the number of bytes in this SinkStream
  size_t Length() const { return buffer_.size(); }

  // Returns a pointer to contiguously allocated Length() bytes in the stream.
  // Writing to the stream invalidates the pointer.  The SinkStream continues to
  // own the memory.
  const uint8_t* Buffer() const {
    return reinterpret_cast<const uint8_t*>(buffer_.data());
  }

  // Hints that the stream will grow by an additional |length| bytes.
  // Caller must be prepared to handle memory allocation problems.
  [[nodiscard]] CheckBool Reserve(size_t length) {
    return buffer_.reserve(length + buffer_.size());
  }

  // Finished with this stream and any storage it has.
  void Retire();

 private:
  NoThrowBuffer<char> buffer_;
};

// A SourceStreamSet is a set of SourceStreams.
class SourceStreamSet {
 public:
  SourceStreamSet();

  SourceStreamSet(const SourceStreamSet&) = delete;
  SourceStreamSet& operator=(const SourceStreamSet&) = delete;

  ~SourceStreamSet();

  // Initializes the SourceStreamSet with the stream data in memory at |source|.
  // The caller continues to own the memory and should not modify or free the
  // memory until the SourceStreamSet destructor has been called.
  //
  // The layout of the streams are as written by SinkStreamSet::CopyTo.
  // Init returns 'false' if the layout is inconsistent with |byte_count|.
  bool Init(const void* source, size_t byte_count);

  // Initializes |this| from |source|.  The caller continues to own the memory
  // because it continues to be owned by |source|.
  bool Init(SourceStream* source);

  // Returns a pointer to one of the sub-streams.
  SourceStream* stream(size_t id) {
    return id < count_ ? &streams_[id] : nullptr;
  }

  // Initialize |set| from |this|.
  bool ReadSet(SourceStreamSet* set);

  // Returns 'true' if all streams are completely consumed.
  bool Empty() const;

 private:
  size_t count_;
  SourceStream streams_[kMaxStreams];
};

// A SinkStreamSet is a set of SinkStreams.  Data is collected by writing to the
// component streams.  When data collection is complete, it is destructively
// transferred, either by flattening into one stream (CopyTo), or transfering
// data pairwise into another SinkStreamSet by calling that SinkStreamSet's
// WriteSet method.
class SinkStreamSet {
 public:
  SinkStreamSet();

  SinkStreamSet(const SinkStreamSet&) = delete;
  SinkStreamSet& operator=(const SinkStreamSet&) = delete;

  ~SinkStreamSet();

  // Initializes the SinkStreamSet to have |stream_index_limit| streams.  Must
  // be <= kMaxStreams.  If Init is not called the default is has kMaxStream.
  void Init(size_t stream_index_limit);

  // Returns a pointer to a substream.
  SinkStream* stream(size_t id) {
    return id < count_ ? &streams_[id] : nullptr;
  }

  // CopyTo serializes the streams in this SinkStreamSet into a single target
  // stream.  The serialized format may be re-read by initializing a
  // SourceStreamSet with a buffer containing the data.
  [[nodiscard]] CheckBool CopyTo(SinkStream* combined_stream);

  // Writes the streams of |set| into the corresponding streams of |this|.
  // Stream zero first has some metadata written to it.  |set| becomes retired.
  // Partner to SourceStreamSet::ReadSet.
  [[nodiscard]] CheckBool WriteSet(SinkStreamSet* set);

 private:
  [[nodiscard]] CheckBool CopyHeaderTo(SinkStream* stream);

  size_t count_;
  SinkStream streams_[kMaxStreams];
};

}  // namespace courgette

#endif  // COURGETTE_STREAMS_H_
