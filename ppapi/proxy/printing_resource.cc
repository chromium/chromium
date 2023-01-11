// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/printing_resource.h"

#include "base/functional/bind.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

PrintingResource::PrintingResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
}

PrintingResource::~PrintingResource() {
}

thunk::PPB_Printing_API* PrintingResource::AsPPB_Printing_API() {
  return this;
}

int32_t PrintingResource::GetDefaultPrintSettings(
    PP_PrintSettings_Dev* print_settings,
    scoped_refptr<TrackedCallback> callback) {
  if (!print_settings)
    return PP_ERROR_BADARGUMENT;

  if (!sent_create_to_browser())
    SendCreate(BROWSER, PpapiHostMsg_Printing_Create());

  Call<PpapiPluginMsg_Printing_GetDefaultPrintSettingsReply>(
      BROWSER, PpapiHostMsg_Printing_GetDefaultPrintSettings(),
      base::BindOnce(&PrintingResource::OnPluginMsgGetDefaultPrintSettingsReply,
                     this, print_settings, callback));
  return PP_OK_COMPLETIONPENDING;
}

void PrintingResource::OnPluginMsgGetDefaultPrintSettingsReply(
    PP_PrintSettings_Dev* settings_out,
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    const PP_PrintSettings_Dev& settings) {
  if (params.result() == PP_OK)
    *settings_out = settings;

  // Notify the plugin of the new data.
  callback->Run(params.result());
  // DANGER: May delete |this|!
}

}  // namespace proxy
}  // namespace ppapi
