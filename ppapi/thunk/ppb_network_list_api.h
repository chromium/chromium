// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_NETWORK_LIST_API_H_
#define PPAPI_THUNK_PPB_NETWORK_LIST_API_H_

#include <stdint.h>

#include "ppapi/c/ppb_network_list.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_NetworkList_API {
 public:
  virtual ~PPB_NetworkList_API() {}

  // Private API
  virtual uint32_t GetCount() = 0;
  virtual PP_Var GetName(uint32_t index) = 0;
  virtual PP_NetworkList_Type GetType(uint32_t index) = 0;
  virtual PP_NetworkList_State GetState(uint32_t index) = 0;
  virtual int32_t GetIpAddresses(uint32_t index,
                                 const PP_ArrayOutput& output) = 0;
  virtual PP_Var GetDisplayName(uint32_t index) = 0;
  virtual uint32_t GetMTU(uint32_t index) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_NETWORK_LIST_API_H_
