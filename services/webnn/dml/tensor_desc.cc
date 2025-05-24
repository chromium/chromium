// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/tensor_desc.h"

#include "base/check.h"
#include "base/check_op.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/webnn_utils.h"

namespace webnn::dml {

TensorDesc::TensorDesc() = default;

TensorDesc::TensorDesc(DML_TENSOR_DATA_TYPE data_type,
                       std::vector<uint32_t> dimensions)
    : TensorDesc(data_type, DML_TENSOR_FLAG_NONE, std::move(dimensions)) {}

TensorDesc::TensorDesc(DML_TENSOR_DATA_TYPE data_type,
                       DML_TENSOR_FLAGS flags,
                       std::vector<uint32_t> dimensions,
                       std::vector<uint32_t> strides) {
  // DML (as of at least 1.11) requires dimension count to be at least 1 because
  // otherwise validation during operator creation will complain with
  // E_INVALIDARG. So scalars must be conveyed with dimensions = [1].
  dimensions_ =
      dimensions.empty() ? std::vector<uint32_t>{1} : std::move(dimensions);

  if (!strides.empty()) {
    CHECK_EQ(dimensions_.size(), strides.size());
  }

  // If no strides are given, set strides as default value calculated from
  // dimensions, e.g., a tensor with dimensions [1, 2, 3, 4] should have default
  // strides [24, 12, 4, 1], referring to
  // https://docs.microsoft.com/en-us/windows/win32/direct3d12/dml-helper-functions#calculatestrides.
  strides_ =
      strides.empty() ? CalculateStrides(dimensions_) : std::move(strides);

  // Round up to the nearest 4 bytes. The buffer allocation already aligned
  // chunks up to DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT.
  uint64_t minimum_implied_size_in_bytes =
      CalculateDMLBufferTensorSize(data_type, dimensions_, strides_);

  buffer_desc_.DimensionCount = dimensions_.size();
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  buffer_desc_.TotalTensorSizeInBytes = minimum_implied_size_in_bytes;
  buffer_desc_.GuaranteedBaseOffsetAlignment = 0;
  buffer_desc_.DataType = data_type;
  buffer_desc_.Flags = flags;

  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};
}

TensorDesc::TensorDesc(TensorDesc const& other)
    : dimensions_(other.dimensions_),
      strides_(other.strides_),
      buffer_desc_(other.buffer_desc_) {
  // Update the internal pointers to dimensions and strides.
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};
}

TensorDesc::TensorDesc(TensorDesc&& other)
    : dimensions_(std::move(other.dimensions_)),
      strides_(std::move(other.strides_)),
      buffer_desc_(std::move(other.buffer_desc_)) {
  // Update the internal pointers to dimensions and strides.
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};
}

TensorDesc& TensorDesc::operator=(const TensorDesc& other) {
  dimensions_ = other.dimensions_;
  strides_ = other.strides_;
  buffer_desc_ = other.buffer_desc_;

  // Update the internal pointers to dimensions and strides.
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};

  return *this;
}

TensorDesc& TensorDesc::operator=(TensorDesc&& other) {
  dimensions_ = std::move(other.dimensions_);
  strides_ = std::move(other.strides_);
  buffer_desc_ = std::move(other.buffer_desc_);

  // Update the internal pointers to dimensions and strides.
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
  tensor_desc_ = DML_TENSOR_DESC{DML_TENSOR_TYPE_BUFFER, &buffer_desc_};

  return *this;
}

TensorDesc::~TensorDesc() = default;

bool TensorDesc::operator==(const TensorDesc& other) const {
  return dimensions_ == other.dimensions_ && strides_ == other.strides_ &&
         buffer_desc_.DataType == other.buffer_desc_.DataType &&
         buffer_desc_.TotalTensorSizeInBytes ==
             other.buffer_desc_.TotalTensorSizeInBytes &&
         buffer_desc_.GuaranteedBaseOffsetAlignment ==
             other.buffer_desc_.GuaranteedBaseOffsetAlignment;
}

void TensorDesc::Transpose(base::span<const uint32_t> permutation) {
  dimensions_ = PermuteArray(dimensions_, permutation);
  strides_ = PermuteArray(strides_, permutation);

  // Round up to the nearest 4 bytes. The buffer allocation already aligned
  // chunks up to DML_MINIMUM_BUFFER_TENSOR_ALIGNMENT.
  uint64_t minimum_implied_size_in_bytes = CalculateDMLBufferTensorSize(
      buffer_desc_.DataType, dimensions_, strides_);
  CHECK_EQ(buffer_desc_.TotalTensorSizeInBytes, minimum_implied_size_in_bytes);

  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
}

// Change the dimensions and strides of the TensorDesc by setting the stride of
// broadcasting dimension to 0 and reuse the stride value for other dimensions,
// its behavior follows the helper function:
// https://learn.microsoft.com/en-us/windows/ai/directml/dml-helper-functions#calculatestrides
void TensorDesc::BroadcastTo(base::span<const uint32_t> broadcasted_dims,
                             size_t ignorable_tails_count) {
  // When broadcasting to a 0D scalar (broadcasted_dims = {}), only the
  // dimension of original tensor equals to {1} is legal, and the dimensions_
  // and strides_ of TensorDesc do not need to be changed at this time.
  if (broadcasted_dims.empty()) {
    CHECK_EQ(ignorable_tails_count, 0u);
    CHECK_EQ(dimensions_.size(), 1u);
    CHECK_EQ(dimensions_[0], 1u);
    return;
  }

  auto original_rank = dimensions_.size(),
       broadcasted_rank = broadcasted_dims.size();
  CHECK_LE(original_rank, broadcasted_rank);

  EnsureMinimumRank(broadcasted_rank, Alignment::kTrailing);

  CHECK_LE(ignorable_tails_count, original_rank);
  for (size_t i = 0; i < broadcasted_rank - ignorable_tails_count; ++i) {
    if (dimensions_[i] != broadcasted_dims[i]) {
      CHECK_EQ(dimensions_[i], 1u);
      dimensions_[i] = broadcasted_dims[i];
      strides_[i] = 0;
    }
  }
}

bool TensorDesc::RightAlignedFlattenTo(size_t flattened_rank) {
  auto flattened_dimensions = dimensions_;
  auto flattened_strides = strides_;
  auto original_rank = dimensions_.size();
  if (flattened_rank == original_rank) {
    return true;
  }
  CHECK_LT(flattened_rank, original_rank);
  CHECK_NE(flattened_rank, 0u);

  auto flattened_size = base::MakeCheckedNum<uint32_t>(1);
  for (size_t i = 0; i < original_rank - flattened_rank + 1; ++i) {
    flattened_size *= flattened_dimensions[i];
  }
  flattened_dimensions.erase(
      flattened_dimensions.begin(),
      flattened_dimensions.begin() + original_rank - flattened_rank);
  flattened_dimensions[0] = flattened_size.ValueOrDie();

  flattened_strides.erase(
      flattened_strides.begin(),
      flattened_strides.begin() + original_rank - flattened_rank);

  // Flattening is invalid if the total tensor size in bytes after flattening
  // does not equal the original size. This can occur when the original strides
  // are non-default values due to broadcasting or transposing.
  if (CalculateDMLBufferTensorSize(buffer_desc_.DataType, flattened_dimensions,
                                   flattened_strides) !=
      buffer_desc_.TotalTensorSizeInBytes) {
    return false;
  }
  dimensions_ = std::move(flattened_dimensions);
  strides_ = std::move(flattened_strides);

  buffer_desc_.DimensionCount = dimensions_.size();
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();

  return true;
}

void TensorDesc::EnsureMinimumRank(size_t minimum_rank, Alignment alignment) {
  if (dimensions_.size() >= minimum_rank) {
    return;
  }

  size_t insertion_count = minimum_rank - dimensions_.size();
  switch (alignment) {
    case Alignment::kLeading: {
      dimensions_.insert(dimensions_.end(), insertion_count, 1u);
      strides_.insert(strides_.end(), insertion_count, 0u);
      break;
    }
    case Alignment::kTrailing: {
      dimensions_.insert(dimensions_.begin(), insertion_count, 1u);
      strides_.insert(strides_.begin(), insertion_count, 0u);
      break;
    }
  }

  // Note the TotalTensorSizeInBytes is not changed by inserting ones in
  // dimensions.
  buffer_desc_.DimensionCount = dimensions_.size();
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
}

void TensorDesc::MakeBroadcastCompatible(size_t minimum_rank,
                                         base::span<const uint32_t> axes) {
  if (dimensions_.size() >= minimum_rank) {
    return;
  }

  CHECK_LE(axes.size(), dimensions_.size());
  std::vector<uint32_t> new_dimensions(minimum_rank, 1);
  std::vector<uint32_t> new_strides(minimum_rank, 0);
  for (size_t i = 0; i < axes.size(); i++) {
    CHECK_LT(axes[i], minimum_rank);
    new_dimensions[axes[i]] = dimensions_[i];
    new_strides[axes[i]] = strides_[i];
  }

  dimensions_ = std::move(new_dimensions);
  strides_ = std::move(new_strides);
  buffer_desc_.DimensionCount = dimensions_.size();
  buffer_desc_.Sizes = dimensions_.data();
  buffer_desc_.Strides = strides_.data();
}

void TensorDesc::SetTotalTensorSizeInBytes(
    uint64_t new_total_tensor_size_bytes) {
  CHECK_GE(new_total_tensor_size_bytes,
           CalculateDMLBufferTensorSize(buffer_desc_.DataType, dimensions_,
                                        strides_));
  CHECK_GE(new_total_tensor_size_bytes, buffer_desc_.TotalTensorSizeInBytes);
  buffer_desc_.TotalTensorSizeInBytes = new_total_tensor_size_bytes;
}
}  // namespace webnn::dml
