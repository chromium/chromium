// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_NETWORK_PROXY_API_H_
#define PPAPI_THUNK_PPB_NETWORK_PROXY_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

struct PP_Var;

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_NetworkProxy_API {
 public:
  virtual ~PPB_NetworkProxy_API() {}

  virtual int32_t GetProxyForURL(PP_Instance instance,
                                 PP_Var url,
                                 PP_Var* proxy_string,
                                 scoped_refptr<TrackedCallback> callback) = 0;

  static const SingletonResourceID kSingletonResourceID =
      NETWORK_PROXY_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_NETWORK_PROXY_API_H_
