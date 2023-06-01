// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_LOGGING_COMMON_PARTIAL_CIRCULAR_BUFFER_H_
#define COMPONENTS_WEBRTC_LOGGING_COMMON_PARTIAL_CIRCULAR_BUFFER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"

namespace webrtc_logging {

// A wrapper around a memory buffer that allows circular read and write with a
// selectable wrapping position. Buffer layout (after wrap; H is header):
// -----------------------------------------------------------
// | H | Beginning           | End           | Middle        |
// -----------------------------------------------------------
//  ^---- Non-wrapping -----^ ^--------- Wrapping ----------^
// The non-wrapping part is never overwritten. The wrapping part will be
// circular. The very first part is the header (see the BufferData struct
// below). It consists of the following information:
// - Length written to the buffer (not including header).
// - Wrapping position.
// - End position of buffer. (If the last byte is at x, this will be x + 1.)
// Users of wrappers around the same underlying buffer must ensure that writing
// is finished before reading is started.
class PartialCircularBuffer {
 public:
  // Use for reading. |buffer_size| is in bytes and must be larger than the
  // header size (see above).
  PartialCircularBuffer(void* buffer, uint32_t buffer_size);

  // Use for writing. |buffer_size| is in bytes and must be larger than the
  // header size (see above). If |append| is true, the header data is not reset
  // and writing will continue were left off, |wrap_position| is then ignored.
  PartialCircularBuffer(void* buffer,
                        uint32_t buffer_size,
                        uint32_t wrap_position,
                        bool append);

  uint32_t Read(void* buffer, uint32_t buffer_size);
  void Write(const void* buffer, uint32_t buffer_size);

 private:
  friend class PartialCircularBufferTest;

#pragma pack(push)
#pragma pack(4)
  struct BufferData {
    uint32_t total_written;
    uint32_t wrap_position;
    uint32_t end_position;
    uint8_t data[1];
  };
#pragma pack(pop)

  void DoWrite(const uint8_t* input, uint32_t input_size);

  // Used for reading and writing.
  raw_ptr<BufferData, DanglingUntriaged> buffer_data_;
  uint32_t memory_buffer_size_;
  uint32_t data_size_;
  uint32_t position_;

  // Used for reading.
  uint32_t total_read_;
};

}  // namespace webrtc_logging

#endif  // COMPONENTS_WEBRTC_LOGGING_COMMON_PARTIAL_CIRCULAR_BUFFER_H_
