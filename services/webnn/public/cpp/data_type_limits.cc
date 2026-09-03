// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/data_type_limits.h"

#include <utility>

namespace webnn {

namespace {

// Overloads so the X-macro below can uniformly remove a data type from both
// `SupportedTensors` and `SupportedDataTypes`.
void RemoveDataTypeFromField(SupportedTensors& tensors,
                             OperandDataType data_type) {
  tensors.data_types.Remove(data_type);
}

void RemoveDataTypeFromField(SupportedDataTypes& data_types,
                             OperandDataType data_type) {
  data_types.Remove(data_type);
}

}  // namespace

DataTypeLimits::DataTypeLimits(mojo::DefaultConstruct::Tag) {}

// The parameter list is generated from `WEBNN_DATA_TYPE_LIMITS_FIELDS`; the
// unnamed trailing `std::nullptr_t` parameter absorbs the trailing comma the
// expansion produces.
#define WEBNN_DATA_TYPE_LIMITS_DEFINE_PARAM(type, name) type name,
DataTypeLimits::DataTypeLimits(WEBNN_DATA_TYPE_LIMITS_FIELDS(
    WEBNN_DATA_TYPE_LIMITS_DEFINE_PARAM) std::nullptr_t /*sentinel*/) {
#undef WEBNN_DATA_TYPE_LIMITS_DEFINE_PARAM
  // Move each parameter into the member of the same name. Assigning in the body
  // rather than an initializer list lets the shared X-macro drive the whole
  // list, and is safe because every member type is default-constructible.
#define WEBNN_DATA_TYPE_LIMITS_INIT_FIELD(type, name) \
  this->name = std::move(name);
  WEBNN_DATA_TYPE_LIMITS_FIELDS(WEBNN_DATA_TYPE_LIMITS_INIT_FIELD)
#undef WEBNN_DATA_TYPE_LIMITS_INIT_FIELD
}

void DataTypeLimits::RemoveDataType(OperandDataType data_type) {
#define WEBNN_DATA_TYPE_LIMITS_REMOVE_FIELD(type, name) \
  RemoveDataTypeFromField(name, data_type);
  WEBNN_DATA_TYPE_LIMITS_FIELDS(WEBNN_DATA_TYPE_LIMITS_REMOVE_FIELD)
#undef WEBNN_DATA_TYPE_LIMITS_REMOVE_FIELD
}

DataTypeLimits::DataTypeLimits(const DataTypeLimits&) = default;
DataTypeLimits& DataTypeLimits::operator=(const DataTypeLimits&) = default;
DataTypeLimits::DataTypeLimits(DataTypeLimits&&) noexcept = default;
DataTypeLimits& DataTypeLimits::operator=(DataTypeLimits&&) noexcept = default;

DataTypeLimits::~DataTypeLimits() = default;

}  // namespace webnn
