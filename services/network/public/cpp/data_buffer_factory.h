// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DATA_BUFFER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DATA_BUFFER_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"

namespace network {

// A generic container for byte data that handles its own memory lifetime.
// This abstraction allows `services/network` APIs to allocate and populate
// data buffers without depending on their underlying storage type. It is
// primarily designed to avoid memory copies when transferring data to Blink.
// Blink can provide a factory that creates buffers backed by `WTF::Vector`,
// which `services/network` fills. These buffers can then be transferred
// seamlessly into a `WTF::SegmentedBuffer` in Blink without reallocating
// or copying the underlying memory.
//
// WARNING: This abstraction and its implementations (e.g.,
// `VectorDataBufferFactory` in Blink) are strictly intended for use with the
// Renderer-Accessible HTTP Cache (crbug.com/473666511) to avoid memory copies
// when reading cached responses directly in the renderer process.
// Do NOT use this for any other networking or loading logic without explicit
// approval from OWNERS.
//
// Note: `uint32_t` is intentionally used for buffer sizes and counts (rather
// than `size_t`) because Blink's `WTF::Vector` size is `wtf_size_t` (defined in
// `third_party/blink/renderer/platform/wtf/wtf_size_t.h` as `uint32_t`),
// avoiding type mismatches and truncation when interoperating with Blink.
class COMPONENT_EXPORT(NETWORK_CPP) DataBuffer {
 public:
  virtual ~DataBuffer() = default;

  // Returns the contents of the buffer as a span.
  virtual base::span<const uint8_t> data() const = 0;
  virtual base::span<uint8_t> data() = 0;
  // Shrinks the buffer size to `size` bytes. `size` must be less than or
  // equal to the current size.
  virtual void Shrink(uint32_t size) = 0;

  // Returns a unique identifier for the specific implementation class.
  // This can be used for safe downcasting when RTTI is disabled.
  virtual const void* GetClassIdentifier() const = 0;
};

// An interface for a collection of DataBuffer objects.
class COMPONENT_EXPORT(NETWORK_CPP) DataBufferList {
 public:
  // Forward iterator that yields base::span<const uint8_t> views of each buffer
  // in the collection.
  class COMPONENT_EXPORT(NETWORK_CPP) Iterator {
   public:
    Iterator();
    Iterator(const DataBufferList* data_buffer_list, uint32_t index);
    Iterator(const Iterator& other);
    Iterator(Iterator&&);
    ~Iterator();
    Iterator& operator=(const Iterator& other);
    Iterator& operator=(Iterator&&);

    Iterator& operator++();
    Iterator operator++(int);

    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

    base::span<const uint8_t> operator*() const;

   private:
    raw_ptr<const DataBufferList> data_buffer_list_ = nullptr;
    uint32_t index_ = 0;
  };

  virtual ~DataBufferList() = default;

  // Appends a DataBuffer to the collection. `buffer` must not be null and must
  // not be empty (calling Append() with a null or empty buffer will
  // CHECK-fail).
  virtual void Append(std::unique_ptr<DataBuffer> buffer) = 0;

  // Returns true if the collection contains no buffers.
  bool empty() const { return size() == 0; }

  // Returns the number of buffers in the collection.
  virtual uint32_t size() const = 0;

  // Returns the data of the buffer at the given `index`.
  // `index` must be less than `size()`.
  virtual base::span<const uint8_t> Get(uint32_t index) const = 0;

  // Returns a unique identifier for the specific implementation class.
  // This can be used for safe downcasting when RTTI is disabled.
  virtual const void* GetClassIdentifier() const = 0;

  // Returns a forward iterator.
  Iterator begin() const;

  // Returns the end iterator.
  Iterator end() const;
};

// Interface for a factory that creates DataBuffer and DataBufferList.
// Strictly intended for use with the Renderer-Accessible HTTP Cache.
class COMPONENT_EXPORT(NETWORK_CPP) DataBufferFactory
    : public base::RefCountedThreadSafe<DataBufferFactory> {
 public:
  // Creates a DataBuffer containing a copy of the given `data`.
  virtual std::unique_ptr<DataBuffer> CreateDataBuffer(
      base::span<const uint8_t> data) = 0;

  // Allocates a new DataBuffer of `size` bytes.
  virtual std::unique_ptr<DataBuffer> AllocateDataBuffer(uint32_t size) = 0;

  // Creates an empty DataBufferList.
  virtual std::unique_ptr<DataBufferList> CreateDataBufferList() = 0;

 protected:
  friend class base::RefCountedThreadSafe<DataBufferFactory>;
  virtual ~DataBufferFactory() = default;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DATA_BUFFER_FACTORY_H_
