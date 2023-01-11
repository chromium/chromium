// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_resolver_resource_base.h"

#include "base/functional/bind.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/error_conversion.h"
#include "ppapi/proxy/net_address_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

HostResolverResourceBase::HostResolverResourceBase(Connection connection,
                                                   PP_Instance instance,
                                                   bool private_api)
    : PluginResource(connection, instance),
      private_api_(private_api),
      allow_get_results_(false) {
  if (private_api)
    SendCreate(BROWSER, PpapiHostMsg_HostResolver_CreatePrivate());
  else
    SendCreate(BROWSER, PpapiHostMsg_HostResolver_Create());
}

HostResolverResourceBase::~HostResolverResourceBase() {
}

int32_t HostResolverResourceBase::ResolveImpl(
    const char* host,
    uint16_t port,
    const PP_HostResolver_Private_Hint* hint,
    scoped_refptr<TrackedCallback> callback) {
  allow_get_results_ = false;
  if (!host || !hint)
    return PP_ERROR_BADARGUMENT;
  if (ResolveInProgress())
    return PP_ERROR_INPROGRESS;

  resolve_callback_ = callback;

  HostPortPair host_port;
  host_port.host = host;
  host_port.port = port;

  SendResolve(host_port, hint);
  return PP_OK_COMPLETIONPENDING;
}

PP_Var HostResolverResourceBase::GetCanonicalNameImpl() {
  if (!allow_get_results_)
    return PP_MakeUndefined();

  return StringVar::StringToPPVar(canonical_name_);
}

uint32_t HostResolverResourceBase::GetSizeImpl() {
  if (!allow_get_results_)
    return 0;
  return static_cast<uint32_t>(net_address_list_.size());
}

scoped_refptr<NetAddressResource> HostResolverResourceBase::GetNetAddressImpl(
    uint32_t index) {
  if (!allow_get_results_ || index >= GetSizeImpl())
    return scoped_refptr<NetAddressResource>();

  return net_address_list_[index];
}

void HostResolverResourceBase::OnPluginMsgResolveReply(
    const ResourceMessageReplyParams& params,
    const std::string& canonical_name,
    const std::vector<PP_NetAddress_Private>& net_address_list) {
  if (params.result() == PP_OK) {
    allow_get_results_ = true;
    canonical_name_ = canonical_name;

    net_address_list_.clear();
    for (std::vector<PP_NetAddress_Private>::const_iterator iter =
             net_address_list.begin();
         iter != net_address_list.end();
         ++iter) {
      net_address_list_.push_back(
          new NetAddressResource(connection(), pp_instance(), *iter));
    }
  } else {
    canonical_name_.clear();
    net_address_list_.clear();
  }
  resolve_callback_->Run(ConvertNetworkAPIErrorForCompatibility(params.result(),
                                                                private_api_));
}

void HostResolverResourceBase::SendResolve(
    const HostPortPair& host_port,
    const PP_HostResolver_Private_Hint* hint) {
  PpapiHostMsg_HostResolver_Resolve msg(host_port, *hint);
  Call<PpapiPluginMsg_HostResolver_ResolveReply>(
      BROWSER, msg,
      base::BindOnce(&HostResolverResourceBase::OnPluginMsgResolveReply,
                     base::Unretained(this)));
}

bool HostResolverResourceBase::ResolveInProgress() const {
  return TrackedCallback::IsPending(resolve_callback_);
}

}  // namespace proxy
}  // namespace ppapi
