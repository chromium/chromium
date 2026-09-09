// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_BASIC_DATA_BUFFER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_BASIC_DATA_BUFFER_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/types/pass_key.h"
#include "services/network/public/cpp/data_buffer_factory.h"

namespace network {

class BasicDataBufferFactory;

// DataBuffer implementation backed by std::vector<uint8_t>.
// Instances are created via BasicDataBufferFactory.
class COMPONENT_EXPORT(NETWORK_CPP) BasicDataBuffer : public DataBuffer {
 public:
  // Use BasicDataBufferFactory to instantiate.
  BasicDataBuffer(base::PassKey<BasicDataBufferFactory>,
                  base::span<const uint8_t> data);
  BasicDataBuffer(base::PassKey<BasicDataBufferFactory>, uint32_t size);
  BasicDataBuffer(const BasicDataBuffer&) = delete;
  BasicDataBuffer& operator=(const BasicDataBuffer&) = delete;
  ~BasicDataBuffer() override;

  // DataBuffer implementation:
  base::span<const uint8_t> data() const override;
  base::span<uint8_t> data() override;
  void Shrink(uint32_t size) override;
  const void* GetClassIdentifier() const override;

  static const void* const kClassIdentifier;

 private:
  std::vector<uint8_t> buffer_;
};

// DataBufferList implementation backed by
// std::vector<std::unique_ptr<DataBuffer>>.
// Instances are created via BasicDataBufferFactory.
class COMPONENT_EXPORT(NETWORK_CPP) BasicDataBufferList
    : public DataBufferList {
 public:
  // Use BasicDataBufferFactory to instantiate.
  explicit BasicDataBufferList(base::PassKey<BasicDataBufferFactory>);
  BasicDataBufferList(const BasicDataBufferList&) = delete;
  BasicDataBufferList& operator=(const BasicDataBufferList&) = delete;
  ~BasicDataBufferList() override;

  // DataBufferList implementation:
  void Append(std::unique_ptr<DataBuffer> buffer) override;
  uint32_t size() const override;
  base::span<const uint8_t> Get(uint32_t index) const override;
  const void* GetClassIdentifier() const override;

  static const void* const kClassIdentifier;

 private:
  std::vector<std::unique_ptr<DataBuffer>> buffers_;
};

// DataBufferFactory implementation for BasicDataBuffer and BasicDataBufferList.
// Provides a standard, vector-backed implementation suitable for general use
// in tests or contexts where no custom buffer backing is required.
class COMPONENT_EXPORT(NETWORK_CPP) BasicDataBufferFactory
    : public DataBufferFactory {
 public:
  BasicDataBufferFactory() = default;
  BasicDataBufferFactory(const BasicDataBufferFactory&) = delete;
  BasicDataBufferFactory& operator=(const BasicDataBufferFactory&) = delete;

  // DataBufferFactory implementation:
  std::unique_ptr<DataBuffer> CreateDataBuffer(
      base::span<const uint8_t> data) override;
  std::unique_ptr<DataBuffer> AllocateDataBuffer(uint32_t size) override;
  std::unique_ptr<DataBufferList> CreateDataBufferList() override;

 protected:
  ~BasicDataBufferFactory() override = default;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_BASIC_DATA_BUFFER_FACTORY_H_
