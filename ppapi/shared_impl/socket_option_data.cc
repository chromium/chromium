// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/socket_option_data.h"

namespace ppapi {

SocketOptionData::SocketOptionData() : type_(TYPE_INVALID), value_(0) {}

SocketOptionData::~SocketOptionData() {}

SocketOptionData::Type SocketOptionData::GetType() const { return type_; }

bool SocketOptionData::GetBool(bool* out_value) const {
  if (!out_value || type_ != TYPE_BOOL)
    return false;
  *out_value = value_ != 0;
  return true;
}

bool SocketOptionData::GetInt32(int32_t* out_value) const {
  if (!out_value || type_ != TYPE_INT32)
    return false;
  *out_value = value_;
  return true;
}

void SocketOptionData::SetBool(bool value) {
  type_ = TYPE_BOOL;
  value_ = value ? 1 : 0;
}

void SocketOptionData::SetInt32(int32_t value) {
  type_ = TYPE_INT32;
  value_ = value;
}

}  // namespace ppapi
