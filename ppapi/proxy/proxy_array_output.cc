// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/proxy_array_output.h"


namespace ppapi {
namespace proxy {

// static
void* ArrayOutputAdapterBase::GetDataBufferThunk(void* user_data,
                                                 uint32_t element_count,
                                                 uint32_t element_size) {
  return static_cast<ArrayOutputAdapterBase*>(user_data)->
      GetDataBuffer(element_count, element_size);
}

}  // namespace proxy
}  // namespace ppapi
