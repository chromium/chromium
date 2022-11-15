// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_HOST_RESOLVER_API_H_
#define PPAPI_THUNK_PPB_HOST_RESOLVER_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/ppb_host_resolver.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_HostResolver_API {
 public:
  virtual ~PPB_HostResolver_API() {}

  virtual int32_t Resolve(const char* host,
                          uint16_t port,
                          const PP_HostResolver_Hint* hint,
                          scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Var GetCanonicalName() = 0;
  virtual uint32_t GetNetAddressCount() = 0;
  virtual PP_Resource GetNetAddress(uint32_t index) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_HOST_RESOLVER_API_H_
