// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_CHUNKED_BUFFER_H_
#define SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_CHUNKED_BUFFER_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"

namespace network {

// WebBundleChunkedBuffer keeps the appended bytes as a RefCountedBytes, so we
// don't need to copy the bytes while creating a DataSource to read the data
// using a DataPipeProducer.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebBundleChunkedBuffer {
 public:
  WebBundleChunkedBuffer();
  ~WebBundleChunkedBuffer();

  // Disallow copy and assign.
  WebBundleChunkedBuffer(const WebBundleChunkedBuffer&) = delete;
  WebBundleChunkedBuffer& operator=(const WebBundleChunkedBuffer&) = delete;

  // Append the bytes as a Chunk.
  void Append(base::span<const uint8_t> data);

  // Returns the available length of bytes after |offset|. If it is larger than
  // |max_length| returns |max_length|,
  uint64_t GetAvailableLength(uint64_t offset, uint64_t max_length) const;

  // Returns whether it contains all of [|offset|, |offset| + |length|) range.
  // When |length| is 0, always returns true.
  bool ContainsAll(uint64_t offset, uint64_t length) const;

  // Read the data to |out|. Returns the total length of the read bytes.
  [[nodiscard]] uint64_t ReadData(uint64_t offset,
                                  base::span<uint8_t> out) const;

  // Creates a DataSource to read the data using a DataPipeProducer. If there
  // is no data to read, returns nullptr.
  std::unique_ptr<mojo::DataPipeProducer::DataSource> CreateDataSource(
      uint64_t offset,
      uint64_t max_length) const;

  // Returns the buffer size.
  uint64_t size() const;

 private:
  friend class WebBundleChunkedBufferTest;
  FRIEND_TEST_ALL_PREFIXES(WebBundleChunkedBufferTest, EmptyBuffer);
  FRIEND_TEST_ALL_PREFIXES(WebBundleChunkedBufferTest, PartialBuffer);
  FRIEND_TEST_ALL_PREFIXES(WebBundleChunkedBufferTest, FindChunk);

  class COMPONENT_EXPORT(NETWORK_SERVICE) Chunk {
   public:
    Chunk(uint64_t start_pos, scoped_refptr<const base::RefCountedBytes> bytes);
    ~Chunk();

    // Allow copy and assign.
    Chunk(const Chunk&);
    Chunk(Chunk&&);
    Chunk& operator=(const Chunk&) = default;
    Chunk& operator=(Chunk&&) = default;

    uint64_t start_pos() const;
    uint64_t end_pos() const;
    size_t size() const;
    const uint8_t* data() const;

   private:
    uint64_t start_pos_;
    scoped_refptr<const base::RefCountedBytes> bytes_;
  };

  using ChunkVector = std::vector<Chunk>;

  explicit WebBundleChunkedBuffer(ChunkVector chunks);

  bool empty() const;
  uint64_t start_pos() const;
  uint64_t end_pos() const;

  // Returns the iterator of |chunks_|, which Chunk contains the byte at |pos|.
  // If |chunks_| doesn't contains the byte at |pos|, returns |chunks_.end()|.
  ChunkVector::const_iterator FindChunk(uint64_t pos) const;

  // Creates a new WebBundleChunkedBuffer which keeps a part of |chunks_|
  // containing the bytes of [|offset|, |offset| + |length|) range.
  std::unique_ptr<const WebBundleChunkedBuffer> CreatePartialBuffer(
      uint64_t offset,
      uint64_t length) const;

  ChunkVector chunks_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_CHUNKED_BUFFER_H_
