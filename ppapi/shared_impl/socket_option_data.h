// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_SOCKET_OPTION_DATA_H_
#define PPAPI_SHARED_IMPL_SOCKET_OPTION_DATA_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT SocketOptionData {
 public:
  enum Type { TYPE_INVALID = 0, TYPE_BOOL = 1, TYPE_INT32 = 2 };

  SocketOptionData();
  ~SocketOptionData();

  Type GetType() const;

  bool GetBool(bool* out_value) const;
  bool GetInt32(int32_t* out_value) const;

  void SetBool(bool value);
  void SetInt32(int32_t value);

 private:
  Type type_;
  int32_t value_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_SOCKET_OPTION_DATA_H_
