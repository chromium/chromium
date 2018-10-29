// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_CORE_SPDY_FRAME_READER_H_
#define NET_THIRD_PARTY_SPDY_CORE_SPDY_FRAME_READER_H_

#include <cstdint>

#include "net/third_party/spdy/platform/api/spdy_export.h"
#include "net/third_party/spdy/platform/api/spdy_string_piece.h"

namespace spdy {

// Used for reading SPDY frames. Though there isn't really anything terribly
// SPDY-specific here, it's a helper class that's useful when doing SPDY
// framing.
//
// To use, simply construct a SpdyFramerReader using the underlying buffer that
// you'd like to read fields from, then call one of the Read*() methods to
// actually do some reading.
//
// This class keeps an internal iterator to keep track of what's already been
// read and each successive Read*() call automatically increments said iterator
// on success. On failure, internal state of the SpdyFrameReader should not be
// trusted and it is up to the caller to throw away the failed instance and
// handle the error as appropriate. None of the Read*() methods should ever be
// called after failure, as they will also fail immediately.
class SPDY_EXPORT_PRIVATE SpdyFrameReader {
 public:
  // Caller must provide an underlying buffer to work on.
  SpdyFrameReader(const char* data, const size_t len);

  // Empty destructor.
  ~SpdyFrameReader() {}

  // Reads an 8-bit unsigned integer into the given output parameter.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadUInt8(uint8_t* result);

  // Reads a 16-bit unsigned integer into the given output parameter.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadUInt16(uint16_t* result);

  // Reads a 32-bit unsigned integer into the given output parameter.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadUInt32(uint32_t* result);

  // Reads a 64-bit unsigned integer into the given output parameter.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadUInt64(uint64_t* result);

  // Reads a 31-bit unsigned integer into the given output parameter. This is
  // equivalent to ReadUInt32() above except that the highest-order bit is
  // discarded.
  // Forwards the internal iterator (by 4B) on success.
  // Returns true on success, false otherwise.
  bool ReadUInt31(uint32_t* result);

  // Reads a 24-bit unsigned integer into the given output parameter.
  // Forwards the internal iterator (by 3B) on success.
  // Returns true on success, false otherwise.
  bool ReadUInt24(uint32_t* result);

  // Reads a string prefixed with 16-bit length into the given output parameter.
  //
  // NOTE: Does not copy but rather references strings in the underlying buffer.
  // This should be kept in mind when handling memory management!
  //
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadStringPiece16(SpdyStringPiece* result);

  // Reads a string prefixed with 32-bit length into the given output parameter.
  //
  // NOTE: Does not copy but rather references strings in the underlying buffer.
  // This should be kept in mind when handling memory management!
  //
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadStringPiece32(SpdyStringPiece* result);

  // Reads a given number of bytes into the given buffer. The buffer
  // must be of adequate size.
  // Forwards the internal iterator on success.
  // Returns true on success, false otherwise.
  bool ReadBytes(void* result, size_t size);

  // Seeks a given number of bytes into the buffer from the current offset.
  // Equivelant to an empty read.
  // Forwards the internal iterator.
  // Returns true on success, false otherwise.
  bool Seek(size_t size);

  // Rewinds this reader to the beginning of the frame.
  void Rewind() { ofs_ = 0; }

  // Returns true if the entirety of the underlying buffer has been read via
  // Read*() calls.
  bool IsDoneReading() const;

  // Returns the number of bytes that have been consumed by the reader so far.
  size_t GetBytesConsumed() const { return ofs_; }

 private:
  // Returns true if the underlying buffer has enough room to read the given
  // amount of bytes.
  bool CanRead(size_t bytes) const;

  // To be called when a read fails for any reason.
  void OnFailure();

  // The data buffer that we're reading from.
  const char* data_;

  // The length of the data buffer that we're reading from.
  const size_t len_;

  // The location of the next read from our data buffer.
  size_t ofs_;
};

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_SPDY_FRAME_READER_H_
