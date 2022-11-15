// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_HOST_RESOLVER_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_HOST_RESOLVER_PRIVATE_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_HostResolver_Private_API {
 public:
  virtual ~PPB_HostResolver_Private_API() {}

  virtual int32_t Resolve(const char* host,
                          uint16_t port,
                          const PP_HostResolver_Private_Hint* hint,
                          scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Var GetCanonicalName() = 0;
  virtual uint32_t GetSize() = 0;
  virtual bool GetNetAddress(uint32_t index,
                             PP_NetAddress_Private* addr) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_HOST_RESOLVER_PRIVATE_API_H_
