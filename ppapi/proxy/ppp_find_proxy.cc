// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_find_proxy.h"

#include "build/build_config.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

#if !BUILDFLAG(IS_NACL)
PP_Bool StartFind(PP_Instance instance,
                  const char* text,
                  PP_Bool case_sensitive) {
  DCHECK(case_sensitive == PP_FALSE);
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiPluginMsg_PPPFind_StartFind(API_ID_PPP_FIND_PRIVATE,
                                           instance,
                                           text));
  return PP_TRUE;
}

void SelectFindResult(PP_Instance instance,
                      PP_Bool forward) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiPluginMsg_PPPFind_SelectFindResult(API_ID_PPP_FIND_PRIVATE,
                                                  instance, forward));
}

void StopFind(PP_Instance instance) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiPluginMsg_PPPFind_StopFind(API_ID_PPP_FIND_PRIVATE, instance));
}

const PPP_Find_Private ppp_find_interface = {
  &StartFind,
  &SelectFindResult,
  &StopFind
};
#else
// The NaCl plugin doesn't need the host side interface - stub it out.
const PPP_Find_Private ppp_find_interface = {};
#endif

}  // namespace

PPP_Find_Proxy::PPP_Find_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_find_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_find_ = static_cast<const PPP_Find_Private*>(
        dispatcher->local_get_interface()(PPP_FIND_PRIVATE_INTERFACE));
  }
}

PPP_Find_Proxy::~PPP_Find_Proxy() {
}

// static
const PPP_Find_Private* PPP_Find_Proxy::GetProxyInterface() {
  return &ppp_find_interface;
}

bool PPP_Find_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Find_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_PPPFind_StartFind, OnPluginMsgStartFind)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_PPPFind_SelectFindResult,
                        OnPluginMsgSelectFindResult)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_PPPFind_StopFind, OnPluginMsgStopFind)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Find_Proxy::OnPluginMsgStartFind(PP_Instance instance,
                                          const std::string& text) {
  if (ppp_find_)
    CallWhileUnlocked(ppp_find_->StartFind, instance, text.c_str(), PP_FALSE);
}

void PPP_Find_Proxy::OnPluginMsgSelectFindResult(PP_Instance instance,
                                                 PP_Bool forward) {
  if (ppp_find_)
    CallWhileUnlocked(ppp_find_->SelectFindResult, instance, forward);
}

void PPP_Find_Proxy::OnPluginMsgStopFind(PP_Instance instance) {
  if (ppp_find_)
    CallWhileUnlocked(ppp_find_->StopFind, instance);
}

}  // namespace proxy
}  // namespace ppapi
