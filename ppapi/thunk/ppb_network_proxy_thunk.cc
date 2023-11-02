// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_network_proxy.idl modified Tue Jun 25 15:45:53 2013.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_network_proxy.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_network_proxy_api.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t GetProxyForURL(PP_Instance instance,
                       struct PP_Var url,
                       struct PP_Var* proxy_string,
                       struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_NetworkProxy::GetProxyForURL()";
  EnterInstanceAPI<PPB_NetworkProxy_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.functions()->GetProxyForURL(
      instance, url, proxy_string, enter.callback()));
}

const PPB_NetworkProxy_1_0 g_ppb_networkproxy_thunk_1_0 = {&GetProxyForURL};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_NetworkProxy_1_0* GetPPB_NetworkProxy_1_0_Thunk() {
  return &g_ppb_networkproxy_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
