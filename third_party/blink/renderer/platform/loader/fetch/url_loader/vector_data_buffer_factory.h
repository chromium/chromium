// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_VECTOR_DATA_BUFFER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_VECTOR_DATA_BUFFER_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/types/pass_key.h"
#include "services/network/public/cpp/data_buffer_factory.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// DataBufferFactory implementation for Blink that produces buffers backed by
// WTF::Vector<char> and lists backed by WTF::SegmentedBuffer. This allows
// network service data to be transferred into Blink's SegmentedBuffer without
// copying memory.
class PLATFORM_EXPORT VectorDataBufferFactory
    : public network::DataBufferFactory {
 public:
  VectorDataBufferFactory();
  VectorDataBufferFactory(const VectorDataBufferFactory&) = delete;
  VectorDataBufferFactory& operator=(const VectorDataBufferFactory&) = delete;
  std::unique_ptr<network::DataBuffer> CreateDataBuffer(
      base::span<const uint8_t> data) override;
  std::unique_ptr<network::DataBuffer> AllocateDataBuffer(
      uint32_t size) override;
  std::unique_ptr<network::DataBufferList> CreateDataBufferList() override;

 protected:
  ~VectorDataBufferFactory() override;
};

// DataBuffer implementation backed by WTF::Vector<char>.
// Instances are created via VectorDataBufferFactory.
class PLATFORM_EXPORT VectorDataBuffer : public network::DataBuffer {
 public:
  VectorDataBuffer(base::PassKey<VectorDataBufferFactory>,
                   base::span<const uint8_t> data);
  VectorDataBuffer(base::PassKey<VectorDataBufferFactory>, uint32_t size);
  VectorDataBuffer(const VectorDataBuffer&) = delete;
  VectorDataBuffer& operator=(const VectorDataBuffer&) = delete;
  ~VectorDataBuffer() override;

  base::span<const uint8_t> data() const override;
  base::span<uint8_t> data() override;
  void Shrink(uint32_t size) override;
  const void* GetClassIdentifier() const override;

  // Moves the underlying WTF::Vector<char> out of this buffer.
  Vector<char> TakeInternalBuffer();

  static const void* const kClassIdentifier;

 private:
  Vector<char> buffer_;
};

// DataBufferList implementation backed by WTF::SegmentedBuffer.
// When appending a VectorDataBuffer, its internal WTF::Vector<char> is moved
// directly into the SegmentedBuffer, avoiding memory copies.
class PLATFORM_EXPORT SegmentedDataBufferList : public network::DataBufferList {
 public:
  explicit SegmentedDataBufferList(base::PassKey<VectorDataBufferFactory>);
  ~SegmentedDataBufferList() override;

  SegmentedDataBufferList(const SegmentedDataBufferList&) = delete;
  SegmentedDataBufferList& operator=(const SegmentedDataBufferList&) = delete;

  // network::DataBufferList implementation:
  void Append(std::unique_ptr<network::DataBuffer> buffer) override;
  uint32_t size() const override;
  base::span<const uint8_t> Get(uint32_t index) const override;
  const void* GetClassIdentifier() const override;

  static const void* const kClassIdentifier;

  // Returns a const reference to the underlying SegmentedBuffer.
  const SegmentedBuffer& GetSegmentedBuffer() const { return buffer_; }

  // Moves and returns the underlying SegmentedBuffer.
  SegmentedBuffer TakeSegmentedBuffer() { return std::move(buffer_); }

 private:
  SegmentedBuffer buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_VECTOR_DATA_BUFFER_FACTORY_H_
