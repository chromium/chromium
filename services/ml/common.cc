// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/common.h"

namespace ml {

uint32_t product(const std::vector<uint32_t>& dims) {
  uint32_t prod = 1;
  for (size_t i = 0; i < dims.size(); ++i)
    prod *= dims[i];
  return prod;
}

uint32_t GetRequiredSize(int32_t type,
                         const std::vector<uint32_t>& dimensions) {
  if (type == mojom::FLOAT32) {
    return sizeof(float);
  } else if (type == mojom::INT32) {
    return sizeof(int32_t);
  } else if (type == mojom::UINT32) {
    return sizeof(uint32_t);
  } else if (type == mojom::TENSOR_FLOAT32) {
    return product(dimensions) * sizeof(float);
  } else if (type == mojom::TENSOR_INT32) {
    return product(dimensions) * sizeof(int32_t);
  } else if (type == mojom::TENSOR_QUANT8_ASYMM) {
    return product(dimensions) * sizeof(int8_t);
  } else {
    NOTREACHED();
  }
  return 0;
}

uint32_t GetRequiredSize(const mojom::OperandPtr& operand) {
  return GetRequiredSize(operand->type, operand->dimensions);
}

uint32_t GetRequiredSize(const mojom::OperandInfoPtr& operand) {
  return GetRequiredSize(operand->type, operand->dimensions);
}

Operand::Operand() = default;
Operand::~Operand() = default;
Operand::Operand(const Operand&) = default;
uint32_t Operand::requiredSize() const {
  return GetRequiredSize(type, dimensions);
}

Operation::Operation() = default;
Operation::~Operation() = default;
Operation::Operation(const Operation&) = default;

OperandInfo::OperandInfo(uint32_t offset,
                         uint32_t length,
                         mojo::ScopedSharedBufferMapping mapping)
    : offset(offset), length(length), mapping(std::move(mapping)) {}

OperandInfo::~OperandInfo() {}

ValueInfo::ValueInfo() = default;
ValueInfo::~ValueInfo() = default;
ValueInfo::ValueInfo(const ValueInfo&) = default;

void PrintOperand(const Operand& operand,
                  const std::unique_ptr<OperandInfo>& info) {
  uint32_t length = info->length;
  if (operand.type == mojom::TENSOR_FLOAT32 || operand.type == mojom::FLOAT32) {
    float* value = static_cast<float*>(info->mapping.get());
    uint32_t size = length / 4;
    DLOG(INFO) << "  "
               << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_INT32 ||
             operand.type == mojom::INT32) {
    int32_t* value = static_cast<int32_t*>(info->mapping.get());
    uint32_t size = length / 4;
    DLOG(INFO) << "  "
               << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::TENSOR_QUANT8_ASYMM) {
    int8_t* value = static_cast<int8_t*>(info->mapping.get());
    uint32_t size = length;
    DLOG(INFO) << "  "
               << "buffer(" << size << "): " << VectorToString(value, size);
  } else if (operand.type == mojom::UINT32) {
    uint32_t* value = static_cast<uint32_t*>(info->mapping.get());
    uint32_t size = length;
    DLOG(INFO) << "  "
               << "buffer(" << size << "): " << VectorToString(value, size);
  }
}

int32_t getScalarInt32(const ValueInfo& info, int8_t* memory) {
  int32_t* ptr = reinterpret_cast<int32_t*>(memory + info.offset);
  return ptr[0];
}

// There are no viable overloaded operator[] for type
// 'const std::map<uint32_t, ValueInfo>', so uses 'find' instead of it.
int32_t getScalarInt32(const std::map<uint32_t, ValueInfo>& values,
                       uint32_t key,
                       int8_t* memory) {
  auto iter = values.find(key);
  if (iter == values.end()) {
    assert(0);
    return -1;
  }

  return getScalarInt32(iter->second, memory);
}

float getScalarFloat(const ValueInfo& info, int8_t* memory) {
  float* ptr = reinterpret_cast<float*>(memory + info.offset);
  return ptr[0];
}

float getScalarFloat(const std::map<uint32_t, ValueInfo>& values,
                     uint32_t key,
                     int8_t* memory) {
  auto iter = values.find(key);
  if (iter == values.end()) {
    assert(0);
    return -1.0;
  }

  return getScalarFloat(iter->second, memory);
}

}  // namespace ml
