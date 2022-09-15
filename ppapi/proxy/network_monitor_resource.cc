// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/network_monitor_resource.h"

#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_network_monitor_api.h"

namespace ppapi {
namespace proxy {

NetworkMonitorResource::NetworkMonitorResource(Connection connection,
                                               PP_Instance instance)
    : PluginResource(connection, instance),
      current_list_(0),
      forbidden_(false),
      network_list_(NULL) {
  SendCreate(BROWSER, PpapiHostMsg_NetworkMonitor_Create());
}

NetworkMonitorResource::~NetworkMonitorResource() {}

ppapi::thunk::PPB_NetworkMonitor_API*
NetworkMonitorResource::AsPPB_NetworkMonitor_API() {
  return this;
}

void NetworkMonitorResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(NetworkMonitorResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_NetworkMonitor_NetworkList, OnPluginMsgNetworkList)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_0(
        PpapiPluginMsg_NetworkMonitor_Forbidden, OnPluginMsgForbidden)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(
        PluginResource::OnReplyReceived(params, msg))
  PPAPI_END_MESSAGE_MAP()
}

int32_t NetworkMonitorResource::UpdateNetworkList(
    PP_Resource* network_list,
    scoped_refptr<TrackedCallback> callback) {
  if (!network_list)
    return PP_ERROR_BADARGUMENT;
  if (TrackedCallback::IsPending(update_callback_))
    return PP_ERROR_INPROGRESS;
  if (forbidden_)
    return PP_ERROR_NOACCESS;

  if (current_list_.get()) {
    *network_list = current_list_.Release();
    return PP_OK;
  }

  network_list_ = network_list;
  update_callback_ = callback;
  return PP_OK_COMPLETIONPENDING;
}

void NetworkMonitorResource::OnPluginMsgNetworkList(
    const ResourceMessageReplyParams& params,
    const SerializedNetworkList& list) {
  current_list_ = ScopedPPResource(
      new NetworkListResource(pp_instance(), list));

  if (TrackedCallback::IsPending(update_callback_)) {
    *network_list_ = current_list_.Release();
    update_callback_->Run(PP_OK);
  }
}

void NetworkMonitorResource::OnPluginMsgForbidden(
    const ResourceMessageReplyParams& params) {
  forbidden_ = true;

  if (TrackedCallback::IsPending(update_callback_))
    update_callback_->Run(PP_ERROR_NOACCESS);
}

}  // namespace proxy
}  // namespace ppapi
