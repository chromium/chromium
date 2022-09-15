// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PRINTING_RESOURCE_H_
#define PPAPI_PROXY_PRINTING_RESOURCE_H_

#include <stdint.h>

#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_printing_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT PrintingResource : public PluginResource,
                                            public thunk::PPB_Printing_API {
 public:
  PrintingResource(Connection connection,
                   PP_Instance instance);

  PrintingResource(const PrintingResource&) = delete;
  PrintingResource& operator=(const PrintingResource&) = delete;

  ~PrintingResource() override;

  // Resource overrides.
  thunk::PPB_Printing_API* AsPPB_Printing_API() override;

  // PPB_Printing_API.
  int32_t GetDefaultPrintSettings(
      PP_PrintSettings_Dev* print_settings,
      scoped_refptr<TrackedCallback> callback) override;

 private:
  void OnPluginMsgGetDefaultPrintSettingsReply(
      PP_PrintSettings_Dev* settings_out,
      scoped_refptr<TrackedCallback> callback,
      const ResourceMessageReplyParams& params,
      const PP_PrintSettings_Dev& settings);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PRINTING_RESOURCE_H_
