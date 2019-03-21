// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMMON_H_
#define SERVICES_ML_COMMON_H_

#include <stdint.h>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "services/ml/public/mojom/constants.mojom.h"
#include "services/ml/public/mojom/execution.mojom.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace ml {

uint32_t product(const std::vector<uint32_t>& dims);

struct Operand {
  Operand();
  ~Operand();
  Operand(const Operand&);
  uint32_t requiredSize() const;
  int32_t type;
  std::vector<uint32_t> dimensions;
  float scale;
  int32_t zeroPoint;
};

struct OperandInfo {
  OperandInfo(uint32_t offset,
              uint32_t length,
              mojo::ScopedSharedBufferMapping mapping);
  ~OperandInfo();
  uint32_t offset;
  uint32_t length;
  mojo::ScopedSharedBufferMapping mapping;
};

struct ValueInfo {
  ValueInfo();
  ~ValueInfo();
  ValueInfo(const ValueInfo&);
  uint32_t index;
  uint32_t offset;
  uint32_t length;
};

struct Operation {
  Operation();
  ~Operation();
  Operation(const Operation&);
  int32_t type;
  std::vector<uint32_t> inputs;
  std::vector<uint32_t> outputs;
};

template <class T>
std::string VectorToString(const T* vect, size_t length) {
  std::string output("[");
  if (length > 200)
    length = 200;
  for (size_t i = 0; i < length; ++i) {
    output.append(base::NumberToString(vect[i]));
    if (i != length - 1) {
      output.append(", ");
    }
  }
  output.append("]");
  return output;
}

void PrintOperand(const Operand& operand,
                  const std::unique_ptr<OperandInfo>& info);

int32_t getScalarInt32(const ValueInfo&, int8_t*);
int32_t getScalarInt32(const std::map<uint32_t, ValueInfo>& values,
                       uint32_t key,
                       int8_t* memory);
float getScalarFloat(const ValueInfo&, int8_t*);
float getScalarFloat(const std::map<uint32_t, ValueInfo>& values,
                     uint32_t key,
                     int8_t*);

uint32_t GetRequiredSize(const mojom::OperandPtr& operand);
uint32_t GetRequiredSize(const mojom::OperandInfoPtr& operand);

}  // namespace ml

#endif  // SERVICES_ML_COMMON_H_