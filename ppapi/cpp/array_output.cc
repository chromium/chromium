// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/array_output.h"

#include "ppapi/cpp/logging.h"

namespace pp {

// static
void* ArrayOutputAdapterBase::GetDataBufferThunk(void* user_data,
                                                 uint32_t element_count,
                                                 uint32_t element_size) {
  return static_cast<ArrayOutputAdapterBase*>(user_data)->
      GetDataBuffer(element_count, element_size);
}

VarArrayOutputAdapterWithStorage::VarArrayOutputAdapterWithStorage()
    : ArrayOutputAdapter<PP_Var>() {
  set_output(&temp_storage_);
}

VarArrayOutputAdapterWithStorage::~VarArrayOutputAdapterWithStorage() {
  if (!temp_storage_.empty()) {
    // An easy way to release the var references held by this object.
    output();
  }
}

std::vector<Var>& VarArrayOutputAdapterWithStorage::output() {
  PP_DCHECK(output_storage_.empty());

  output_storage_.reserve(temp_storage_.size());
  for (size_t i = 0; i < temp_storage_.size(); i++)
    output_storage_.push_back(Var(PASS_REF, temp_storage_[i]));
  temp_storage_.clear();
  return output_storage_;
}

}  // namespace pp
