// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_INFLATER_H_
#define NET_WEBSOCKETS_WEBSOCKET_INFLATER_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"

extern "C" struct z_stream_s;

namespace net {

class IOBufferWithSize;

// WebSocketInflater uncompresses data compressed by DEFLATE algorithm.
class NET_EXPORT_PRIVATE WebSocketInflater {
 public:
  WebSocketInflater();
  // |input_queue_capacity| is a capacity for each contiguous block in the
  // input queue. The input queue can grow without limit.
  WebSocketInflater(size_t input_queue_capacity, size_t output_buffer_capacity);

  WebSocketInflater(const WebSocketInflater&) = delete;
  WebSocketInflater& operator=(const WebSocketInflater&) = delete;

  ~WebSocketInflater();

  // Returns true if there is no error.
  // |window_bits| must be between 8 and 15 (both inclusive).
  // This function must be called exactly once before calling any of the
  // following functions.
  bool Initialize(int window_bits);

  // Adds bytes to |stream_|.
  // Returns true if there is no error.
  // If the size of the output data reaches the capacity of the output buffer,
  // the following input data will be "choked", i.e. stored in the input queue,
  // staying compressed.
  bool AddBytes(const char* data, size_t size);

  // Flushes the input.
  // Returns true if there is no error.
  bool Finish();

  // Returns up to |size| bytes of the decompressed output.
  // Returns null if there is an inflation error.
  // The returned bytes will be dropped from the current output and never be
  // returned again.
  // If some input data is choked, calling this function may restart the
  // inflation process.
  // This means that even if you call |Finish()| and call |GetOutput()| with
  // size = |CurrentOutputSize()|, the inflater may have some remaining data.
  // To confirm the inflater emptiness, you should check whether
  // |CurrentOutputSize()| is zero.
  scoped_refptr<IOBufferWithSize> GetOutput(size_t size);

  // Returns the size of the current inflated output.
  size_t CurrentOutputSize() const { return output_buffer_.Size(); }

  static constexpr size_t kDefaultBufferCapacity = 512;
  static constexpr size_t kDefaultInputIOBufferCapacity = 512;

 private:
  // Ring buffer with fixed capacity.
  class NET_EXPORT_PRIVATE OutputBuffer {
   public:
    explicit OutputBuffer(size_t capacity);
    ~OutputBuffer();

    size_t Size() const;
    // Returns (tail pointer, availabe size).
    // A user can push data to the queue by writing the data to
    // the area returned by this function and calling AdvanceTail.
    std::pair<char*, size_t> GetTail();
    void Read(char* dest, size_t size);
    void AdvanceTail(size_t advance);

   private:
    void AdvanceHead(size_t advance);

    const size_t capacity_;
    std::vector<char> buffer_;
    size_t head_ = 0;
    size_t tail_ = 0;
  };

  class InputQueue {
   public:
    // |capacity| is used for the capacity of each IOBuffer in this queue.
    // this queue itself can grow without limit.
    explicit InputQueue(size_t capacity);
    ~InputQueue();

    // Returns (data pointer, size), the first component of unconsumed data.
    // The type of data pointer is non-const because |inflate| function
    // requires so.
    std::pair<char*, size_t> Top();
    bool IsEmpty() const { return buffers_.empty(); }
    void Push(const char* data, size_t size);
    // Consumes the topmost |size| bytes.
    // |size| must be less than or equal to the first buffer size.
    void Consume(size_t size);

   private:
    size_t PushToLastBuffer(const char* data, size_t size);

    const size_t capacity_;
    size_t head_of_first_buffer_ = 0;
    size_t tail_of_last_buffer_ = 0;
    base::circular_deque<scoped_refptr<IOBufferWithSize>> buffers_;
  };

  int InflateWithFlush(const char* next_in, size_t avail_in);
  int Inflate(const char* next_in, size_t avail_in, int flush);
  int InflateChokedInput();

  std::unique_ptr<z_stream_s> stream_;
  InputQueue input_queue_;
  OutputBuffer output_buffer_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_INFLATER_H_
