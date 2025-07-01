// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/network_proxy_resource.h"

#include "base/functional/bind.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

NetworkProxyResource::NetworkProxyResource(Connection connection,
                                           PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(BROWSER, PpapiHostMsg_NetworkProxy_Create());
}

NetworkProxyResource::~NetworkProxyResource() {
}

thunk::PPB_NetworkProxy_API* NetworkProxyResource::AsPPB_NetworkProxy_API() {
  return this;
}

int32_t NetworkProxyResource::GetProxyForURL(
      PP_Instance /* instance */,
      PP_Var url,
      PP_Var* proxy_string,
      scoped_refptr<TrackedCallback> callback) {
  StringVar* string_url = StringVar::FromPPVar(url);
  if (!string_url)
    return PP_ERROR_BADARGUMENT;
  Call<PpapiPluginMsg_NetworkProxy_GetProxyForURLReply>(
      BROWSER, PpapiHostMsg_NetworkProxy_GetProxyForURL(string_url->value()),
      base::BindOnce(&NetworkProxyResource::OnPluginMsgGetProxyForURLReply,
                     base::Unretained(this), base::Unretained(proxy_string),
                     callback));
  return PP_OK_COMPLETIONPENDING;
}

void NetworkProxyResource::OnPluginMsgGetProxyForURLReply(
    PP_Var* proxy_string_out_param,
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    const std::string& proxy_string) {
  if (!TrackedCallback::IsPending(callback)) {
    // The callback should not have already been run. If this resource is
    // deleted, LastPluginRefWasReleased in PluginResource should abort the
    // callback and should not run this callback.
    NOTREACHED();
  }
  if (params.result() == PP_OK) {
    *proxy_string_out_param = (new StringVar(proxy_string))->GetPPVar();
  }
  callback->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
