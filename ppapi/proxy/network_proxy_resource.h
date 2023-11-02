// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_NETWORK_PROXY_RESOURCE_H_
#define PPAPI_PROXY_NETWORK_PROXY_RESOURCE_H_

#include <stdint.h>

#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_network_proxy_api.h"

namespace ppapi {
namespace proxy {

// The proxy-side resource for PPB_NetworkProxy.
class PPAPI_PROXY_EXPORT NetworkProxyResource
      : public PluginResource,
        public thunk::PPB_NetworkProxy_API {
 public:
  NetworkProxyResource(Connection connection, PP_Instance instance);

  NetworkProxyResource(const NetworkProxyResource&) = delete;
  NetworkProxyResource& operator=(const NetworkProxyResource&) = delete;

  ~NetworkProxyResource() override;

 private:
  // Resource implementation.
  thunk::PPB_NetworkProxy_API* AsPPB_NetworkProxy_API() override;

  // PPB_NetworkProxy_API implementation.
  int32_t GetProxyForURL(
      PP_Instance instance,
      PP_Var url,
      PP_Var* proxy_string,
      scoped_refptr<TrackedCallback> callback) override;

  void OnPluginMsgGetProxyForURLReply(PP_Var* proxy_string_out_param,
                                      scoped_refptr<TrackedCallback> callback,
                                      const ResourceMessageReplyParams& params,
                                      const std::string& proxy_string);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_NETWORK_PROXY_RESOURCE_H_
