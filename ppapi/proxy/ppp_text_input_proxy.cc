// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_text_input_proxy.h"

#include "build/build_config.h"
#include "ppapi/c/dev/ppp_text_input_dev.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

#if !BUILDFLAG(IS_NACL)
void RequestSurroundingText(PP_Instance instance,
                            uint32_t desired_number_of_characters) {
  proxy::HostDispatcher* dispatcher =
      proxy::HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    // The dispatcher should always be valid.
    NOTREACHED();
  }

  dispatcher->Send(new PpapiMsg_PPPTextInput_RequestSurroundingText(
      API_ID_PPP_TEXT_INPUT, instance, desired_number_of_characters));
}

const PPP_TextInput_Dev g_ppp_text_input_thunk = {
  &RequestSurroundingText
};
#else
// The NaCl plugin doesn't need the host side interface - stub it out.
static const PPP_TextInput_Dev g_ppp_text_input_thunk = {};
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace

// static
const PPP_TextInput_Dev* PPP_TextInput_Proxy::GetProxyInterface() {
  return &g_ppp_text_input_thunk;
}

PPP_TextInput_Proxy::PPP_TextInput_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_text_input_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_text_input_impl_ = static_cast<const PPP_TextInput_Dev*>(
        dispatcher->local_get_interface()(PPP_TEXTINPUT_DEV_INTERFACE));
  }
}

PPP_TextInput_Proxy::~PPP_TextInput_Proxy() {
}

bool PPP_TextInput_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_TextInput_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPTextInput_RequestSurroundingText,
                        OnMsgRequestSurroundingText)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_TextInput_Proxy::OnMsgRequestSurroundingText(
    PP_Instance instance, uint32_t desired_number_of_characters) {
  if (ppp_text_input_impl_) {
    CallWhileUnlocked(ppp_text_input_impl_->RequestSurroundingText,
                      instance, desired_number_of_characters);
  }
}

}  // namespace proxy
}  // namespace ppapi
