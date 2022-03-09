// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_pdf_proxy.h"

#include "build/build_config.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

#if !BUILDFLAG(IS_NACL)
int32_t PrintBegin(PP_Instance instance,
                   const PP_PrintSettings_Dev* print_settings,
                   const PP_PdfPrintSettings_Dev* pdf_print_settings) {
  int32_t ret = 0;
  HostDispatcher::GetForInstance(instance)->Send(new PpapiMsg_PPPPdf_PrintBegin(
      API_ID_PPP_PDF, instance, *print_settings, *pdf_print_settings, &ret));
  return ret;
}

const PPP_Pdf ppp_pdf_interface = {
    &PrintBegin,
};
#else
// The NaCl plugin doesn't need the host side interface - stub it out.
const PPP_Pdf ppp_pdf_interface = {};
#endif

}  // namespace

PPP_Pdf_Proxy::PPP_Pdf_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_pdf_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_pdf_ = static_cast<const PPP_Pdf*>(
        dispatcher->local_get_interface()(PPP_PDF_INTERFACE));
  }
}

PPP_Pdf_Proxy::~PPP_Pdf_Proxy() {
}

// static
const PPP_Pdf* PPP_Pdf_Proxy::GetProxyInterface() {
  return &ppp_pdf_interface;
}

bool PPP_Pdf_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Pdf_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPdf_PrintBegin, OnPluginMsgPrintBegin)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Pdf_Proxy::OnPluginMsgPrintBegin(
    PP_Instance instance,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings,
    int32_t* result) {
  if (!ppp_pdf_) {
    *result = 0;
    return;
  }

  *result = CallWhileUnlocked(ppp_pdf_->PrintBegin, instance, &print_settings,
                              &pdf_print_settings);
}

}  // namespace proxy
}  // namespace ppapi
