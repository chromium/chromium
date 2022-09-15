// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_URL_RESPONSE_INFO_RESOURCE_H_
#define PPAPI_PROXY_URL_RESPONSE_INFO_RESOURCE_H_

#include "base/compiler_specific.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/url_response_info_data.h"
#include "ppapi/thunk/ppb_url_response_info_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT URLResponseInfoResource
    : public PluginResource,
      public thunk::PPB_URLResponseInfo_API {
 public:
  URLResponseInfoResource(Connection connection,
                          PP_Instance instance,
                          const URLResponseInfoData& data);

  URLResponseInfoResource(const URLResponseInfoResource&) = delete;
  URLResponseInfoResource& operator=(const URLResponseInfoResource&) = delete;

  ~URLResponseInfoResource() override;

  // Resource override.
  PPB_URLResponseInfo_API* AsPPB_URLResponseInfo_API() override;

  // PPB_URLResponseInfo_API implementation.
  PP_Var GetProperty(PP_URLResponseProperty property) override;
  PP_Resource GetBodyAsFileRef() override;

  const URLResponseInfoData& data() const { return data_; }

 private:
  URLResponseInfoData data_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_URL_RESPONSE_INFO_RESOURCE_H_
