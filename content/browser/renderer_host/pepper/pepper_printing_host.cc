// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_printing_host.h"

#include <utility>

#include "base/bind.h"
#include "ppapi/c/dev/pp_print_settings_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

PepperPrintingHost::PepperPrintingHost(
    ppapi::host::PpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    std::unique_ptr<PepperPrintSettingsManager> print_settings_manager)
    : ResourceHost(host, instance, resource),
      print_settings_manager_(std::move(print_settings_manager)) {}

PepperPrintingHost::~PepperPrintingHost() {}

int32_t PepperPrintingHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperPrintingHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_Printing_GetDefaultPrintSettings,
        OnGetDefaultPrintSettings)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperPrintingHost::OnGetDefaultPrintSettings(
    ppapi::host::HostMessageContext* context) {
  print_settings_manager_->GetDefaultPrintSettings(
      base::Bind(&PepperPrintingHost::PrintSettingsCallback,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext()));
  return PP_OK_COMPLETIONPENDING;
}

void PepperPrintingHost::PrintSettingsCallback(
    ppapi::host::ReplyMessageContext reply_context,
    PepperPrintSettingsManager::Result result) {
  reply_context.params.set_result(result.second);
  host()->SendReply(
      reply_context,
      PpapiPluginMsg_Printing_GetDefaultPrintSettingsReply(result.first));
}

}  // namespace content
